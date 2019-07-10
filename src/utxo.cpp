#include "utxo.h"
#include "caterpillar.h"

uint256 XOR(const uint256& hash, uint32_t index) {
    return ArithToUint256(UintToArith256(hash) ^ (arith_uint256(index) << 224));
}

//////////////////////
// UTXO
//
uint256 UTXO::GetContainingBlkHash() const {
    return output_.GetParentTx()->GetParentBlock()->GetHash();
}

uint256 UTXO::GetKey() const {
    return XOR(GetContainingBlkHash(), index_);
}

uint64_t UTXO::HashCode() const {
    return std::hash<uint256>()(GetContainingBlkHash()) ^ index_;
}

//////////////////////
// TXOC
//
void TXOC::AddToCreated(const UTXOPtr& putxo) {
    increment_.Create(putxo->GetKey());
}

void TXOC::AddToCreated(const uint256& blkHash, uint32_t index) {
    increment_.Create(XOR(blkHash, index));
}

void TXOC::AddToSpent(const TxInput& input) {
    auto& outpoint = input.outpoint;
    increment_.Remove(XOR(outpoint.bHash, outpoint.index));
}

void TXOC::Merge(TXOC&& txoc) {
    increment_.Merge(std::move(txoc.increment_));
}

//////////////////////
// ChainLedger
//
void ChainLedger::AddToPending(UTXOPtr putxo) {
    pending_.insert({putxo->GetKey(), putxo});
}

UTXOPtr ChainLedger::GetFromPending(const uint256& xorkey) {
    auto query = pending_.find(xorkey);
    if (query != pending_.end()) {
        return query->second;
    }
    return nullptr;
}

UTXOPtr ChainLedger::FindSpendable(const uint256& xorkey) {
    auto query = removed_.find(xorkey);
    if (query != removed_.end()) {
        return nullptr; // nullptr as it is found in map of removed utxos
    }
    query = comfirmed_.find(xorkey);
    if (query != comfirmed_.end()) {
        return query->second;
    }
    return CAT->GetUTXO(xorkey);
}

void ChainLedger::Update(const TXOC& txoc) {
    for (const auto& utxokey : txoc.GetCreated()) {
        comfirmed_.insert(pending_.extract(utxokey));
    }
    for (const auto& utxokey : txoc.GetSpent()) {
        removed_.insert(comfirmed_.extract(utxokey));
    }
}

void ChainLedger::Rollback(const TXOC& txoc) {
    for (const auto& utxokey : txoc.GetCreated()) {
        pending_.insert(comfirmed_.extract(utxokey));
    }
    for (const auto& utxokey : txoc.GetSpent()) {
        comfirmed_.insert(removed_.extract(utxokey));
    }
}

//////////////////////
// to_string methods
//
std::string std::to_string(const UTXO& utxo) {
    std::string s;
    s += "UTXO { \n";
    s += strprintf("   outpoint: %s:%s\n", std::to_string(utxo.GetContainingBlkHash()), utxo.index_);
    s += "   " + std::to_string(utxo.output_);
    s += "   }";
    return s;
}

std::string std::to_string(const TXOC& txoc) {
    std::string s;
    s += "TXOC { \n";
    for (const auto& utxo : txoc.GetCreated()) {
        s += std::to_string(utxo);
    }
    for (const auto& utxo : txoc.GetSpent()) {
        s += std::to_string(utxo) + "\n";
    }
    s += "   }";
    return s;
}
