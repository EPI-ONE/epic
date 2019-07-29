#include <utility>

#include "mempool.h"
#include "wallet.h"
#include "mempool.h"

#include <algorithm>
#include <numeric>

void Wallet::Start() {
    threadPool.Start();
}

void Wallet::Stop() {
    threadPool.Stop();
}

void Wallet::OnLvsConfirmed(std::vector<RecordPtr> records,
                            std::unordered_set<UTXOPtr> UTXOs,
                            std::unordered_set<uint256> STXOs) {
    for (auto& record : records) {
        ProcessRecord(record);
    }
    for (auto& stxo : STXOs) {
        ProcessSTXO(stxo);
    }
    for (auto& utxo : UTXOs) {
        ProcessUTXO(utxo);
    }
}

void Wallet::ProcessUTXO(const UTXOPtr& utxo) {
    std::string addrString;
    const auto content = utxo->GetOutput().listingContent;
    VStream stream(content.data);
    stream >> addrString;
    auto keyId = DecodeAddress(addrString);
    if (keyId) {
        if (keyBook.find(*keyId) != keyBook.end()) {
            unspent.insert(
                {utxo->GetKey(), std::make_tuple(*keyId, utxo->GetIndex(), utxo->GetOutput().value.GetValue())});
            totalBalance += utxo->GetOutput().value.GetValue();
        }
    }
}

void Wallet::ProcessSTXO(const UTXOKey& stxo) {
    auto it = pending.find(stxo);
    if (it != pending.end()) {
        spent.insert((*it));
        pending.erase(it);
    }
}

void Wallet::ProcessRecord(const RecordPtr& record) {
    // update miner info
    if (record->cblock->source == Block::Source::MINER) {
        minerInfo_ = {record->cblock->GetHash(), record->cumulativeReward};
    }

    if (!record->cblock->HasTransaction()) {
        return;
    }

    auto it = pendingTx.find(record->cblock->GetTransaction()->GetHash());
    if (it == pendingTx.end()) {
        return;
    }

    if (record->validity != record->VALID) {
        spdlog::warn("[Wallet] tx failed to be confirmed, block hash = {}", std::to_string(record->cblock->GetHash()));
    }

    pendingTx.erase(it);
    // TODO trace tx history
}

CKeyID Wallet::CreateNewKey(bool compressed) {
    CKey privkey{};
    privkey.MakeNewKey(compressed);
    CPubKey pubkey = privkey.GetPubKey();
    auto addr      = pubkey.GetID();

    keyBook.emplace(addr, std::pair{std::move(privkey), std::move(pubkey)});
    return addr;
}

TxInput Wallet::CreateSignedVin(CKeyID targetAddr, TxOutPoint outpoint, std::string& msg) {
    // get keys and sign
    const auto& [privkey, pubkey] = keyBook.at(targetAddr);
    auto hashMsg                  = Hash<1>(msg.cbegin(), msg.cend());
    std::vector<unsigned char> sig;
    privkey.Sign(hashMsg, sig);

    // return prepared input
    return TxInput{outpoint, Tasm::Listing{VStream{pubkey, sig, hashMsg}}};
}

ConstTxPtr Wallet::CreateRedemption(CKeyID targetAddr, CKeyID nextAddr, std::string msg) {
    Transaction redeem{};
    redeem.AddInput(CreateSignedVin(targetAddr, TxOutPoint{minerInfo_.first, UNCONNECTED}, msg))
        .AddOutput(minerInfo_.second, nextAddr);

    return std::make_shared<const Transaction>(std::move(redeem));
}

ConstTxPtr Wallet::CreateFirstRegistration(CKeyID address) {
    return std::make_shared<const Transaction>(address);
}

ConstTxPtr Wallet::CreateTx(std::vector<std::pair<Coin, CKeyID>> outputs, const Coin& fee) {
    auto totalCost = std::accumulate(outputs.begin(), outputs.end(), Coin{0},
                                     [](Coin prev, auto pair) { return prev + pair.first; });
    totalCost      = totalCost + (fee == 0 ? MIN_FEE : fee);
    if (totalCost > totalBalance) {
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
    auto tx_ptr = std::make_shared<const Transaction>(tx);
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

void Wallet::AddInput(Transaction& tx, utxo_info& utxo) {
    auto index     = std::get<OUTPUT_INDEX>(utxo.second);
    auto blockHash = XOR(utxo.first, index);
    auto& keyID    = std::get<CKeyID>(utxo.second);
    std::string message("wallet_create_new_transaction");
    tx.AddInput(CreateSignedVin(keyID, TxOutPoint(blockHash, index), message));
    SpendUTXO(utxo.first);
}

void Wallet::SpendUTXO(UTXOKey utxoKey) {
    auto it = unspent.find(utxoKey);
    if (it != unspent.end()) {
        totalBalance -= std::get<COIN>(it->second);
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
    keyBook.insert_or_assign(pubKey.GetID(), {key, pubKey});
}

bool Wallet::SendTxToMemPool(ConstTxPtr txPtr) {
    if (!MEMPOOL) {
        return false;
    }
    return MEMPOOL->Insert(std::move(txPtr));
}

Coin Wallet::GetCurrentMinerReward() const {
    return minerInfo_.second;
}
