#include "wallet.h"
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
    for (auto& utxo : UTXOs) {
        ProcessUTXO(utxo);
    }
    for (auto& stxo : STXOs) {
        ProcessSTXO(stxo);
    }
    for (auto& record : records) {
        ProcessRecord(record);
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

    keyBook.emplace(addr, std::move(privkey), std::move(pubkey));
    return addr;
}

TxInput Wallet::CreateSignedVin(CKeyID targetAddr, TxOutPoint outpoint, std::string& msg) {
    // get keys and sign
    const auto& [privkey, pubkey] = keyBook.at(targetAddr);
    auto hashMsg                  = Hash<1>(msg.cbegin(), msg.cend());
    std::vector<unsigned char> sig;
    privkey.Sign(hashMsg, sig);

    // return prepared input
    return TxInput{outpoint, Tasm::Listing{VStream{pubkey, hashMsg, sig}}};
}

ConstTxPtr Wallet::CreateRedemption(CKeyID targetAddr, CKeyID nextAddr, std::string& msg) {
    // TODO: fetch data of a least value that can be redeemed
    Coin redeemValue = 0;
    Transaction redeem{};
    redeem.AddInput(CreateSignedVin(targetAddr, TxOutPoint{minerRedeemHash_, UNCONNECTED}, msg))
        .AddOutput(redeemValue, nextAddr);

    return std::make_shared<const Transaction>(std::move(redeem));
}

ConstTxPtr Wallet::CreateTx(std::vector<std::pair<Coin, CKeyID>> outputs) {
    auto totalCost = std::accumulate(outputs.begin(), outputs.end(), Coin{0},
                                     [](Coin prev, auto pair) { return prev + pair.first; });
    totalCost      = totalCost + MIN_FEE;
    if (totalCost > totalBalance) {
        spdlog::warn("[Wallet] too much money that we can't afford");
        return nullptr;
    }

    std::vector<utxo_info> utxo_list(unspent.begin(), unspent.end());
    std::sort(utxo_list.begin(), utxo_list.end(), [](const utxo_info& lhs, const utxo_info& rhs) {
        return std::get<COIN>(lhs.second) > std::get<COIN>(rhs.second);
    });

    Transaction tx;

    std::vector<UTXOKey> inputsFrom;
    uint64_t totalInputMoney = 0;

    for (auto& utxo : utxo_list) {
        totalInputMoney += std::get<COIN>(utxo.second);
        AddInput(tx, utxo);
        if (totalCost <= totalInputMoney) {
            break;
        }
    }

    for (auto& output : outputs) {
        tx.AddOutput(output.first, output.second);
    }
    tx.FinalizeHash();
    auto tx_ptr = std::make_shared<const Transaction>(tx);
    pendingTx.insert({tx_ptr->GetHash(), tx_ptr});
    return tx_ptr;
}

void Wallet::AddInput(Transaction& tx, utxo_info& utxo) {
    auto index     = std::get<OUTPUT_INDEX>(utxo.second);
    auto blockHash = XOR(utxo.first, index);
    auto& keyID    = std::get<CKeyID>(utxo.second);
    auto& key      = keyBook.find(keyID)->second.first;
    auto hashMsg   = uint256S("4de04506f44155e2a59d2e8af4e6e15e9f50f5f0b1dc7a0742021799981180c2");
    std::vector<unsigned char> sig;
    key.Sign(hashMsg, sig);
    tx.AddSignedInput(TxOutPoint(blockHash, index), keyBook.find(keyID)->second.second, hashMsg, sig);
    SpendUTXO(utxo.first);
}

void Wallet::SpendUTXO(UTXOKey utxoKey) {
    auto it = unspent.find(utxoKey);
    if (it != unspent.end()) {
        totalBalance -= std::get<COIN>(it->second);
        unspent.erase(utxoKey);
    }
}

void Wallet::GenerateNewKey() {
    CKey key;
    key.MakeNewKey(true);
    auto pubkey = key.GetPubKey();
    keyBook.insert({pubkey.GetID(), {key, pubkey}});
}

CKeyID Wallet::GetRandomAddress() {
    if (keyBook.empty()) {
        GenerateNewKey();
    }
    return keyBook.begin()->first;
}

void Wallet::ImportKey(CKey& key, CPubKey& pubKey) {
    keyBook.insert_or_assign(pubKey.GetID(), {key, pubKey});
>>>>>>> c74ebf1... create tx by wallet
}
