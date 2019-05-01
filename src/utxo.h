#ifndef __SRC_UTXO_H__
#define __SRC_UTXO_H__

#include "block.h"

class UTXO : TxOutput {
private:
    uint32_t index_;

public:
    UTXO(const TxOutput& output, const uint32_t& index) : TxOutput(output), index_(index) {}

    /*
     * Key for searching in maps or in DB: hash ^ index
     * This methods is called immediately after the tx containing this output is validated,
     * and this utxo is then stored in a map in Chain
     */
    uint256 GetKey() {
        const Transaction* tx = GetParentTx();
        const Block* b        = tx->GetParentBlock();
        uint256 h             = b->GetHash();

        return ArithToUint256(
            UintToArith256(GetParentTx()->GetParentBlock()->GetHash()) ^ (arith_uint256(index_) << 224));
    }

    uint64_t HashCode() const {
        return std::hash<uint256>()(GetParentTx()->GetParentBlock()->GetHash()) ^ index_;
    }
};

template <>
struct std::hash<UTXO> {
    size_t operator()(const UTXO& u) const {
        return u.HashCode();
    }
};
#endif /* ifndef __SRC_UTXO_H__ */

