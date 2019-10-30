// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "wallet.h"
#include "mempool.h"
#include "random.h"

#include <algorithm>
#include <chrono>
#include <numeric>
#include <thread>
#include <tuple>
#include <utility>

void Wallet::Start() {
    stopFlag_ = false;
    threadPool_.Start();

    // zero period means no storing
    if (storePeriod_ != 0) {
        scheduleTask_ = std::thread([&]() { SendPeriodicTasks(storePeriod_, 20); });
    }
}

void Wallet::Stop() {
    spdlog::info("Stopping wallet...");
    stopFlag_ = true;
    if (scheduleTask_.joinable()) {
        scheduleTask_.join();
    }
    threadPool_.Stop();
    spdlog::info("Wallet stopped.");
}

Wallet::~Wallet() {
    Stop();
    spdlog::trace("Destructing wallet");
}

void Wallet::Load() {
    // an old wallet should always have keys
    auto keysMap = walletStore_.GetAllKey();
    if (keysMap.empty()) {
        return;
    }

    for (auto& [addr, encryptedPair] : keysMap) {
        keyBook.emplace(addr, std::make_pair(std::get<0>(encryptedPair), std::get<1>(encryptedPair)));
    }

    if (auto info = walletStore_.GetMasterInfo()) {
        masterInfo_  = std::move(*info);
        cryptedFlag_ = true;
    }

    hasSentFirstRegistration_ = walletStore_.GetFirstRegInfo();
    if (auto last_redem_addr = walletStore_.GetLastRedemAddr()) {
        lastRedemAddress_ = *last_redem_addr;
    }

    pendingTx = walletStore_.GetAllTx();
    unspent   = walletStore_.GetAllUnspent();
    pending   = walletStore_.GetAllPending();

    size_t balance = 0;
    for (auto& utxoPair : unspent) {
        balance += std::get<COIN>(utxoPair.second);
    }
    totalBalance_ = balance;
}

void Wallet::SendPeriodicTasks(uint32_t storagePeriod, uint32_t loginSession) {
    // periodic task of storing data
    scheduler_.AddPeriodTask(storagePeriod, [&]() {
        threadPool_.Execute([&]() {
            walletStore_.ClearOldData();
            spdlog::debug("Back up wallet data...");

            for (const auto& pairTx : pendingTx) {
                walletStore_.StoreTx(*pairTx.second);
            }
            for (const auto& [utxoKey, utxoTuple] : unspent) {
                walletStore_.StoreUnspent(utxoKey, std::get<CKEY_ID>(utxoTuple), std::get<TX_INDEX>(utxoTuple),
                                          std::get<OUTPUT_INDEX>(utxoTuple), std::get<COIN>(utxoTuple));
            }
            for (const auto& [utxoKey, utxoTuple] : pending) {
                walletStore_.StorePending(utxoKey, std::get<CKEY_ID>(utxoTuple), std::get<TX_INDEX>(utxoTuple),
                                          std::get<OUTPUT_INDEX>(utxoTuple), std::get<COIN>(utxoTuple));
            }
        });
    });

    scheduler_.AddPeriodTask(loginSession, [&]() {
        threadPool_.Execute([&]() {
            if (rpcLoggedin_) {
                rpcLoggedin_ = false;
                spdlog::debug("wallet login session expired!");
            }
        });
    });

    while (!stopFlag_) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        scheduler_.Loop();
    }
}

void Wallet::OnLvsConfirmed(std::vector<VertexPtr> vertices,
                            std::unordered_map<uint256, UTXOPtr> UTXOs,
                            std::unordered_set<uint256> STXOs) {
    threadPool_.Execute([=, vertices = std::move(vertices), UTXOs = std::move(UTXOs), STXOs = std::move(STXOs)]() {
        for (auto& vertex : vertices) {
            ProcessVertex(vertex);
        }
        for (auto& [utxokey, utxo] : UTXOs) {
            ProcessUTXO(utxokey, utxo);
        }
        for (auto& stxo : STXOs) {
            ProcessSTXO(stxo);
        }

        // for (auto& u : unspent) {
        // std::cout << "unspent " << std::get<3>(u.second) << std::endl;
        //}
    });

    static Coin rewards = GetParams().reward * RedemptionInterval;
    if (enableRedem_.load() && CanRedeem(rewards)) {
        CreateRedemption(CreateNewKey(true));
    }
}

void Wallet::ProcessUTXO(const uint256& utxokey, const UTXOPtr& utxo) {
    auto keyId = ParseAddrFromScript(utxo->GetOutput().listingContent);
    if (keyId) {
        if (keyBook.find(*keyId) != keyBook.end()) {
            auto indices = utxo->GetIndices();
            unspent.insert(
                {utxokey, std::make_tuple(*keyId, indices.first, indices.second, utxo->GetOutput().value.GetValue())});
            totalBalance_ += utxo->GetOutput().value.GetValue();
        }
    }
}

void Wallet::ProcessSTXO(const UTXOKey& stxo) {
    auto it = pending.find(stxo);
    if (it != pending.end()) {
        auto& tup = it->second;
        walletStore_.StoreSpent(it->first, std::get<CKEY_ID>(tup), std::get<TX_INDEX>(tup), std::get<OUTPUT_INDEX>(tup),
                                std::get<COIN>(tup));
        pending.erase(it);
    }
}

void Wallet::ProcessVertex(const VertexPtr& vertex) {
    // update miner info
    if (vertex->cblock->source == Block::Source::MINER) {
        UpdateMinerInfo(vertex->cblock->GetHash(), vertex->cumulativeReward);
        if (vertex->cblock->IsFirstRegistration()) {
            walletStore_.StoreFirstRegInfo();
        }
    }

    if (!vertex->cblock->HasTransaction()) {
        return;
    }

    const auto& txns = vertex->cblock->GetTransactions();
    for (size_t i = 0; i < txns.size(); ++i) {
        const auto& tx       = txns[i];
        const auto& validity = vertex->validity[i];

        auto it = pendingTx.find(tx->GetHash());
        if (it != pendingTx.end()) {
            if (validity != Vertex::Validity::VALID) {
                spdlog::warn("[Wallet] Tx failed to be confirmed, block hash = {}",
                             std::to_string(vertex->cblock->GetHash()));

                // release all relavent utxos from pending
                for (auto& input : tx->GetInputs()) {
                    auto utxoKey =
                        ComputeUTXOKey(input.outpoint.bHash, input.outpoint.txIndex, input.outpoint.outIndex);
                    auto it_pending = pending.find(utxoKey);
                    if (it_pending != pending.end()) {
                        unspent.insert(*it_pending);
                        totalBalance_ += std::get<COIN>(it_pending->second);
                        pending.erase(it_pending);
                    }
                }
            }
            pendingTx.erase(it);
            continue;
        }

        it = pendingRedemption.find(tx->GetHash());
        if (it != pendingRedemption.end()) {
            if (validity == Vertex::Validity::VALID) {
                auto output = it->second->GetOutputs()[0];
                auto keyId  = ParseAddrFromScript(output.listingContent);
                assert(keyId);
                SetLastRedemAddress(*keyId);
                SetLastRedemHash(vertex->cblock->GetHash());
            }
            pendingRedemption.erase(it);
        }
    }
}

CKeyID Wallet::CreateNewKey(bool compressed) {
    CKey privkey{};
    privkey.MakeNewKey(compressed);
    CPubKey pubkey = privkey.GetPubKey();
    auto addr      = pubkey.GetID();

    CiphertextKey ciphertext;
    crypter_.EncryptKey(pubkey, privkey, ciphertext);

    walletStore_.StoreKeys(addr, ciphertext, pubkey);
    keyBook.emplace(addr, std::make_pair(ciphertext, pubkey));
    return addr;
}

TxInput Wallet::CreateSignedVin(const CKeyID& targetAddr, TxOutPoint outpoint, const std::string& msg) {
    auto hashMsg = HashSHA2<1>(msg.data(), msg.size());
    // get keys and sign
    CiphertextKey ciphertext;
    CPubKey pubkey;
    try {
        std::tie(ciphertext, pubkey) = keyBook.at(targetAddr);
    } catch (std::exception& e) {
        std::cout << e.what() << " when creating signed vin\n";
    }
    CKey privkey{};
    if (!crypter_.DecryptKey(pubkey, ciphertext, privkey)) {
        spdlog::error("[Wallet] Fail to decrypt private keys");
        return TxInput{};
    }

    std::vector<unsigned char> sig;
    privkey.Sign(hashMsg, sig);

    // return prepared input
    return TxInput{outpoint, pubkey, hashMsg, sig};
}

ConstTxPtr Wallet::CreateRedemption(const CKeyID& targetAddr, const CKeyID& nextAddr, const std::string& msg) {
    Transaction redeem{};
    auto minerInfo = GetMinerInfo();
    redeem.AddInput(CreateSignedVin(targetAddr, TxOutPoint{GetLastRedemHash(), UNCONNECTED, UNCONNECTED}, msg))
        .AddOutput(minerInfo.second, nextAddr);
    auto redemPtr = std::make_shared<const Transaction>(std::move(redeem));
    return redemPtr;
}

void Wallet::CreateRedemption(const CKeyID& key) {
    assert(!key.IsNull());

    auto redem = CreateRedemption(GetLastRedemAddress(), key, "lalala");
    pendingRedemption.insert({redem->GetHash(), redem});
    MEMPOOL->PushRedemptionTx(redem);
    spdlog::info("[Wallet] Created redemption of reward {} coins: {}", redem->GetOutputs()[0].value.GetValue(),
                 std::to_string(redem->GetHash()));
}

std::string Wallet::CreateFirstRegistration(const std::optional<CKeyID>& oaddr) {
    CKeyID addr;
    if (!oaddr) {
        addr = CreateNewKey(true);
    } else {
        addr = *oaddr;
    }

    // Reset miner info and clear redemption caches
    minerInfo_ = {uint256{}, 0};
    pendingRedemption.clear();
    MEMPOOL->ClearRedemptions();
    lastRedemHash_.SetNull();
    lastRedemAddress_.SetNull();

    auto reg = std::make_shared<const Transaction>(static_cast<const CKeyID>(addr));
    pendingRedemption.insert({reg->GetHash(), reg});
    MEMPOOL->PushRedemptionTx(reg);

    hasSentFirstRegistration_ = true;
    spdlog::info("[Wallet] Created first registration {}", std::to_string(reg->GetHash()));

    return EncodeAddress(addr);
}

std::string Wallet::CreateFirstRegWhenPossible(const std::optional<CKeyID>& oaddr) {
    if (hasSentFirstRegistration_) {
        return "";
    }

    return CreateFirstRegistration(oaddr);
}

ConstTxPtr Wallet::CreateTx(const std::vector<std::pair<Coin, CKeyID>>& outputs, const Coin& fee) {
    auto totalCost = std::accumulate(outputs.begin(), outputs.end(), Coin{0},
                                     [](Coin prev, auto pair) { return prev + pair.first; });
    totalCost      = totalCost + (fee < MIN_FEE ? MIN_FEE : fee);
    if (totalCost > totalBalance_) {
        spdlog::info("[Wallet] too much money that we can't afford. Current balance = {}", totalBalance_);
        return nullptr;
    }

    auto [totalInput, toSpend] = Select(totalCost);
    Transaction tx;

    for (auto& utxo : toSpend) {
        AddInput(tx, utxo);
    }

    for (auto& output : outputs) {
        tx.AddOutput(output.first, output.second);
    }

    if (totalInput - totalCost > 0) {
        tx.AddOutput(totalInput - totalCost, GetRandomAddress());
    }

    tx.FinalizeHash();
    auto tx_ptr = std::make_shared<const Transaction>(std::move(tx));
    pendingTx.insert({tx_ptr->GetHash(), tx_ptr});
    return tx_ptr;
}

std::pair<Coin, std::vector<Wallet::utxo_info>> Wallet::Select(const Coin& amount) const {
    std::vector<utxo_info> utxo_list = unspent.dump_to_vector();
    std::sort(utxo_list.begin(), utxo_list.end(), [](const utxo_info& lhs, const utxo_info& rhs) {
        return std::get<COIN>(lhs.second) > std::get<COIN>(rhs.second);
    });
    std::vector<utxo_info> result;
    Coin totalInput{0};
    for (auto& utxo : utxo_list) {
        totalInput += std::get<COIN>(utxo.second);
        result.emplace_back(utxo);
        if (totalInput >= amount) {
            break;
        }
    }
    return std::make_pair(totalInput, result);
}

void Wallet::AddInput(Transaction& tx, const utxo_info& utxo) {
    auto txIndex   = std::get<TX_INDEX>(utxo.second);
    auto outIndex  = std::get<OUTPUT_INDEX>(utxo.second);
    auto blockHash = ComputeUTXOKey(utxo.first, txIndex, outIndex);
    auto& keyID    = std::get<CKeyID>(utxo.second);
    std::string message("wallet_create_new_transaction");
    tx.AddInput(CreateSignedVin(keyID, TxOutPoint(blockHash, txIndex, outIndex), message));
    SpendUTXO(utxo.first);
}

void Wallet::SpendUTXO(const UTXOKey& utxoKey) {
    auto it = unspent.find(utxoKey);
    if (it != unspent.end()) {
        totalBalance_ -= std::get<COIN>(it->second);
        pending.emplace(*it);
        unspent.erase(utxoKey);
    }
}

CKeyID Wallet::GetRandomAddress() {
    if (keyBook.empty()) {
        CreateNewKey(true);
    }
    return keyBook.begin()->first;
}

void Wallet::ImportKey(const CKey& key, const CPubKey& pubKey) {
    CiphertextKey ciphertext;
    auto addr = pubKey.GetID();
    crypter_.EncryptKey(pubKey, key, ciphertext);
    walletStore_.StoreKeys(addr, ciphertext, pubKey);
    keyBook.insert_or_assign(addr, {ciphertext, pubKey});
}

bool Wallet::SendTxToMemPool(ConstTxPtr txPtr) {
    if (!MEMPOOL) {
        return false;
    }
    return MEMPOOL->Insert(std::move(txPtr));
}

Coin Wallet::GetCurrentMinerReward() const {
    return GetMinerInfo().second;
}

bool Wallet::CanRedeem(Coin coins) {
    return GetMinerInfo().second >= coins && pendingRedemption.empty();
}

void Wallet::CreateRandomTx(size_t sizeTx) {
    threadPool_.Execute([=, size = sizeTx]() {
        for (int i = 0; i < size; i++) {
            if (stopFlag_) {
                return;
            }
            auto addr = CreateNewKey(true);
            if (!CreateFirstRegWhenPossible(addr).empty()) {
                continue;
            }
            if (GetBalance() <= (MIN_FEE + 1)) {
                if (CanRedeem()) {
                    CreateRedemption(addr);
                    continue;
                } else {
                    while (GetBalance() <= MIN_FEE && !CanRedeem()) {
                        // give up creating tx if we don't have enough balance nor can't redeem
                        std::this_thread::yield();
                        if (stopFlag_) {
                            return;
                        }
                    }
                    // try again to create redemption or normal transaction
                    i--;
                    continue;
                }
            } else {
                Coin coin{random() % (GetBalance() - MIN_FEE - 2).GetValue() + MIN_FEE + 1};
                auto tx = CreateTx({{coin, addr}});
                SendTxToMemPool(tx);
                spdlog::info("[Wallet] Sent {} coins to {} in tx {} with index {}", coin.GetValue(),
                             EncodeAddress(addr), std::to_string(tx->GetHash()), i);
            }
        }
        spdlog::info("Creating {} random transaction task completed", sizeTx);
    });
}

void Wallet::UpdateMinerInfo(uint256 blockHash, const Coin& value) {
    WRITER_LOCK(lock_)
    minerInfo_.first  = blockHash;
    minerInfo_.second = value;
}

std::pair<uint256, Coin> Wallet::GetMinerInfo() const {
    READER_LOCK(lock_)
    return minerInfo_;
}

CKeyID Wallet::GetLastRedemAddress() const {
    READER_LOCK(lock_)
    return lastRedemAddress_;
}

void Wallet::SetLastRedemAddress(const CKeyID& lastRedemAddress) {
    WRITER_LOCK(lock_)
    lastRedemAddress_ = lastRedemAddress;
    walletStore_.StoreLastRedemAddr(lastRedemAddress_);
}

uint256 Wallet::GetLastRedemHash() const {
    READER_LOCK(lock_);
    return lastRedemHash_;
}

void Wallet::SetLastRedemHash(const uint256& h) {
    WRITER_LOCK(lock_)
    lastRedemHash_ = h;
}

ConstTxPtr Wallet::CreateTxAndSend(const std::vector<std::pair<Coin, CKeyID>>& outputs, const Coin& fee) {
    auto tx = CreateTx(outputs, fee);
    SendTxToMemPool(tx);
    return tx;
}

std::optional<CKeyID> ParseAddrFromScript(const Tasm::Listing& content) {
    std::string addrString;
    VStream stream(content.data);
    stream >> addrString;
    return DecodeAddress(addrString);
}

inline uint64_t CurrentTimeInMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::high_resolution_clock::now().time_since_epoch())
        .count();
}

bool Wallet::SetPassphrase(const SecureString& phrase) {
    if (IsCrypted()) {
        return false;
    }
    GetRDRandBytes(masterInfo_.salt.data(), masterInfo_.salt.size());

    // get the number of rounds to cost about 0.1 second
    auto start = CurrentTimeInMs();
    crypter_.SetKeyFromPassphrase(phrase, masterInfo_.salt, 25000);
    masterInfo_.nDeriveIterations = static_cast<unsigned int>(25000 * 100 / (CurrentTimeInMs() - start));

    start = CurrentTimeInMs();
    crypter_.SetKeyFromPassphrase(phrase, masterInfo_.salt, masterInfo_.nDeriveIterations);
    masterInfo_.nDeriveIterations =
        (static_cast<unsigned int>(masterInfo_.nDeriveIterations * 100 / (CurrentTimeInMs() - start)) +
         masterInfo_.nDeriveIterations) /
        2;

    if (masterInfo_.nDeriveIterations < 25000) {
        masterInfo_.nDeriveIterations = 25000;
    }

    if (!crypter_.SetKeyFromPassphrase(phrase, masterInfo_.salt, masterInfo_.nDeriveIterations)) {
        return false;
    }
    if (!crypter_.EncryptMaster(masterInfo_.cryptedMaster)) {
        return false;
    }

    // store master information
    walletStore_.StoreMasterInfo(masterInfo_);
    // TODO: set HD wallet
    cryptedFlag_ = true;
    return crypter_.IsReady();
}

bool Wallet::ChangePassphrase(const SecureString& oldPhrase, const SecureString& newPhrase) {
    if (!IsCrypted()) {
        return false;
    }

    Crypter oldCrypter{};
    if (auto oCrypter = CheckPassphraseMatch(oldPhrase)) {
        oldCrypter = *oCrypter;
    } else {
        return false;
    }

    cryptedFlag_ = false;
    if (!SetPassphrase(newPhrase)) {
        return false;
    }

    // change all encrypted content
    for (auto it = keyBook.begin(); it != keyBook.end(); it++) {
        auto value = it->second;
        CKey priv{};
        oldCrypter.DecryptKey(value.second, value.first, priv);
        CiphertextKey newCiphterText{};
        crypter_.EncryptKey(value.second, priv, newCiphterText);
        value.first = newCiphterText;

        if (!walletStore_.StoreKeys(it->first, value.first, value.second)) {
            spdlog::error("[Wallet] Fail to store key with key-id {}", it->first.GetHex());
        }
        std::tie(it, std::ignore) = keyBook.insert_or_assign(it->first, std::move(value));
    }

    return true;
}

bool Wallet::CheckPassphrase(const SecureString& phrase) {
    if (auto oCrypter = CheckPassphraseMatch(phrase)) {
        if (!master_.empty()) {
            return true;
        } else {
            // when there is no available master key, i.e. loading data, we do decryption to get one
            if (!oCrypter->IsReady()) {
                return false;
            }

            crypter_           = std::move(*oCrypter);
            const auto& master = crypter_.GetMaster();
            master_.resize(master.size());
            memcpy(master_.data(), master.data(), master.size());
            return crypter_.IsReady();
        }
    } else {
        return false;
    }
}

std::optional<Crypter> Wallet::CheckPassphraseMatch(const SecureString& phrase) const {
    Crypter tmpcrypter{};
    if (!tmpcrypter.SetKeyFromPassphrase(phrase, masterInfo_.salt, masterInfo_.nDeriveIterations)) {
        return {};
    }

    if (master_.empty() && tmpcrypter.DecryptMaster(masterInfo_.cryptedMaster)) {
        return tmpcrypter;
    }

    std::vector<unsigned char> ciphertext{};
    tmpcrypter.SetMaster(master_);
    if (tmpcrypter.EncryptMaster(ciphertext) &&
        memcmp(ciphertext.data(), masterInfo_.cryptedMaster.data(), ciphertext.size()) == 0) {
        return tmpcrypter;
    }
    return {};
}

bool Wallet::IsCrypted() {
    return cryptedFlag_ && crypter_.IsReady();
}

bool Wallet::GenerateMaster() {
    Mnemonics mne;
    if (!mne.Generate()) {
        return false;
    }
    // TODO: add mnemonics printing
    // mne.PrintToFile(CONFIG->GetWalletPath());
    std::tie(master_, chaincode_) = mne.GetMasterKeyAndSeed();
    crypter_.SetMaster(master_);
    return true;
}
