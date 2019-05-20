#include "utxo.h"

uint256 HashXORIndex(const uint256& hash, uint32_t index) {
    return ArithToUint256(UintToArith256(hash) ^ (arith_uint256(index) << 224));
}

void TXOC::AddToCreated(UTXO&& utxo) {
    created_.emplace_back(utxo);
}

void TXOC::AddToCreated(const TxOutput& output, uint32_t index) {
    created_.emplace_back(UTXO(output, index));
}

void TXOC::AddToSpent(const TxInput& input) {
    auto outpoint = input.outpoint;
    spent_.emplace_back(HashXORIndex(outpoint.bHash, outpoint.index));
}

std::string std::to_string(const UTXO& utxo) {
    std::string s;
    s += "UTXO { \n";
    s += "   " + std::to_string(utxo.output_);
    s += strprintf("   index: %d\n", utxo.index_);
    s += "   }";
    return s;
}

std::string std::to_string(const TXOC& txoc) {
    std::string s;
    s += "TXOC { \n";
    for (const auto& utxo: txoc.created_) {
        s += std::to_string(utxo);
    }
    for (const auto& utxo: txoc.spent_) {
        s += std::to_string(utxo);
    }
    s += "   }";
    return s;
}
