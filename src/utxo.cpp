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
void TXOC::AddToCreated(UTXO&& utxo) {
    created_.emplace_back(utxo);
}

void TXOC::AddToCreated(const TxOutput& output, uint32_t index) {
    created_.emplace_back(UTXO(output, index));
}

void TXOC::AddToSpent(const UTXO& utxo) {
    spent_.emplace_back(utxo.GetKey());
}

void TXOC::AddToSpent(const TxInput& input) {
    auto outpoint = input.outpoint;
    spent_.emplace_back(XOR(outpoint.bHash, outpoint.index));
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
