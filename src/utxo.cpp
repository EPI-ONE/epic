#include "utxo.h"

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
    created_.emplace(putxo->GetKey());
}

void TXOC::AddToCreated(const uint256& blkHash, uint32_t index) {
    created_.emplace(XOR(blkHash, index));
}

void TXOC::AddToSpent(const TxInput& input) {
    auto& outpoint = input.outpoint;
    spent_.emplace(XOR(outpoint.bHash, outpoint.index));
}

void TXOC::Merge(TXOC&& another) {
    created_.merge(std::move(another.created_));
    for (const auto& utxokey : another.spent_) {
        if (created_.erase(utxokey) == 0) {
            spent_.emplace(utxokey);
        }
    }
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
        return nullptr;
    }
    query = comfirmed_.find(xorkey);
    if (query != comfirmed_.end()) {
        return query->second;
    }
    return nullptr;
}

void ChainLedger::Update(const TXOC& txoc) {
    for (const auto& utxokey : txoc.GetTxOutsCreated()) {
        comfirmed_.insert(pending_.extract(utxokey));
    }
    for (const auto& utxokey : txoc.GetTxOutsSpent()) {
        removed_.insert(comfirmed_.extract(utxokey));
    }
}

void ChainLedger::Rollback(const TXOC& txoc) {
    for (const auto& utxokey : txoc.GetTxOutsCreated()) {
        pending_.insert(comfirmed_.extract(utxokey));
    }
    for (const auto& utxokey : txoc.GetTxOutsSpent()) {
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
    for (const auto& utxo : txoc.created_) {
        s += std::to_string(utxo);
    }
    for (const auto& utxo : txoc.spent_) {
        s += std::to_string(utxo) + "\n";
    }
    s += "   }";
    return s;
}
