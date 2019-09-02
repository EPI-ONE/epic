#include "wallet.h"
#include "mempool.h"

#include <algorithm>
#include <chrono>
#include <numeric>
#include <thread>
#include <utility>

void Wallet::Start() {
    stopFlag_ = false;
    threadPool_.Start();

    // zero period means no storing
    if (storePeriod_ != 0) {
        scheduleTask_ = std::thread([&]() { SendingStorageTask(storePeriod_); });
    }
}

void Wallet::Stop() {
    stopFlag_ = true;
    if (scheduleTask_.joinable()) {
        scheduleTask_.join();
    }
    threadPool_.Stop();
}

void Wallet::Load() {
    // an old wallet should always have keys
    auto keys = walletStore_.GetAllKey();
    if (keys.empty()) {
        return;
    }

    for (auto& [addr, key] : keys) {
        keyBook.emplace(addr, std::pair{key, key.GetPubKey()});
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

void Wallet::SendingStorageTask(uint32_t period) {
    // period task of storing data
    scheduler_.AddPeriodTask(period, [&]() {
        threadPool_.Execute([&]() {
            walletStore_.ClearOldData();

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

    while (!stopFlag_) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        scheduler_.Loop();
    }
}

void Wallet::OnLvsConfirmed(std::vector<RecordPtr> records,
                            std::unordered_map<uint256, UTXOPtr> UTXOs,
                            std::unordered_set<uint256> STXOs) {
    threadPool_.Execute([=, records = std::move(records), UTXOs = std::move(UTXOs), STXOs = std::move(STXOs)]() {
        for (auto& record : records) {
            ProcessRecord(record);
        }
        for (auto& [utxokey, utxo] : UTXOs) {
            ProcessUTXO(utxokey, utxo);
        }
        for (auto& stxo : STXOs) {
            ProcessSTXO(stxo);
        }
    });
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

void Wallet::ProcessRecord(const RecordPtr& record) {
    // update miner info
    if (record->cblock->source == Block::Source::MINER) {
        UpdateMinerInfo(record->cblock->GetHash(), record->cumulativeReward);
    }

    if (!record->cblock->HasTransaction()) {
        return;
    }

    const auto& txns = record->cblock->GetTransactions();
    for (size_t i = 0; i < txns.size(); ++i) {
        const auto& tx       = txns[i];
        const auto& validity = record->validity[i];

        auto it = pendingTx.find(tx->GetHash());
        if (it != pendingTx.end()) {
            if (validity != record->VALID) {
                spdlog::warn("[Wallet] tx failed to be confirmed, block hash = {}",
                             std::to_string(record->cblock->GetHash()));

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
            auto output = it->second->GetOutputs()[0];
            auto keyId  = ParseAddrFromScript(output.listingContent);
            assert(keyId);
            SetLastRedemAddress(*keyId);
            pendingRedemption.erase(it);
        }
    }
}

CKey Wallet::CreateNewKey(bool compressed) {
    CKey privkey{};
    privkey.MakeNewKey(compressed);
    CPubKey pubkey = privkey.GetPubKey();
    auto addr      = pubkey.GetID();

    walletStore_.StoreKeys(addr, privkey);
    keyBook.emplace(addr, std::pair{privkey, pubkey});
    return privkey;
}

TxInput Wallet::CreateSignedVin(const CKeyID& targetAddr, TxOutPoint outpoint, const std::string& msg) {
    // get keys and sign
    const auto& [privkey, pubkey] = keyBook.at(targetAddr);
    auto hashMsg                  = HashSHA2<1>(msg.data(), msg.size());
    std::vector<unsigned char> sig;
    privkey.Sign(hashMsg, sig);

    // return prepared input
    return TxInput{outpoint, pubkey, hashMsg, sig};
}

ConstTxPtr Wallet::CreateRedemption(const CKeyID& targetAddr, const CKeyID& nextAddr, const std::string& msg) {
    Transaction redeem{};
    auto minerInfo = GetMinerInfo();
    redeem.AddInput(CreateSignedVin(targetAddr, TxOutPoint{minerInfo.first, UNCONNECTED, UNCONNECTED}, msg))
        .AddOutput(minerInfo.second, nextAddr);
    auto redemPtr = std::make_shared<const Transaction>(std::move(redeem));
    pendingRedemption.insert({redemPtr->GetHash(), redemPtr});
    return redemPtr;
}

ConstTxPtr Wallet::CreateFirstRegistration(const CKeyID& address) {
    auto reg                  = std::make_shared<const Transaction>(address);
    hasSentFirstRegistration_ = true;
    SetLastRedemAddress(address);
    return reg;
}

ConstTxPtr Wallet::CreateTx(const std::vector<std::pair<Coin, CKeyID>>& outputs, const Coin& fee) {
    auto totalCost = std::accumulate(outputs.begin(), outputs.end(), Coin{0},
                                     [](Coin prev, auto pair) { return prev + pair.first; });
    totalCost      = totalCost + (fee < MIN_FEE ? MIN_FEE : fee);
    if (totalCost > totalBalance_) {
        spdlog::warn("[Wallet] too much money that we can't afford");
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
    std::vector<utxo_info> utxo_list(unspent.begin(), unspent.end());
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
    return {totalInput, result};
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
        CreateNewKey(false);
    }
    return keyBook.begin()->first;
}

void Wallet::ImportKey(CKey& key, CPubKey& pubKey) {
    walletStore_.StoreKeys(pubKey.GetID(), key);
    keyBook.insert_or_assign(pubKey.GetID(), {key, pubKey});
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

bool Wallet::CanRedeem() {
    return GetMinerInfo().second > 0 && pendingRedemption.empty();
}

void Wallet::CreateRandomTx(size_t sizeTx) {
    threadPool_.Execute([=, size = sizeTx]() {
        for (int i = 0; i < size; i++) {
            if (stopFlag_) {
                return;
            }
            auto addr = CreateNewKey(false).GetPubKey().GetID();
            if (!hasSentFirstRegistration_) {
                auto reg = CreateFirstRegistration(addr);
                spdlog::info("[Wallet] Created first registration {}, index = {}", std::to_string(reg->GetHash()), i);
                MEMPOOL->PushRedemptionTx(reg);
                continue;
            }
            if (GetBalance() <= MIN_FEE) {
                if (CanRedeem()) {
                    auto redem = CreateRedemption(GetLastRedemAddress(), addr, "lalala");
                    spdlog::info("[Wallet] Created redemption {}, index = {}", std::to_string(redem->GetHash()), i);
                    pendingRedemption.insert({redem->GetHash(), redem});
                    MEMPOOL->PushRedemptionTx(redem);
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
                Coin coin{random() % (GetBalance() - MIN_FEE).GetValue()};
                auto tx = CreateTx({{coin, addr}});
                SendTxToMemPool(tx);
                spdlog::info("[Wallet] Created tx {}, index = {}", std::to_string(tx->GetHash()), i);
            }
        }
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

const CKeyID Wallet::GetLastRedemAddress() const {
    READER_LOCK(lock_)
    return lastRedemAddress_;
}

void Wallet::SetLastRedemAddress(const CKeyID& lastRedemAddress) {
    WRITER_LOCK(lock_)
    Wallet::lastRedemAddress_ = lastRedemAddress;
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
