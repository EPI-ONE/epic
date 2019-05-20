#ifndef __SRC_UTXO_H__
#define __SRC_UTXO_H__

#include "block.h"
static constexpr uint64_t UNSPENT = UINT_LEAST64_MAX;

class UTXO;
class TXOC;

namespace std {
string to_string(const UTXO&);
string to_string(const TXOC&);
} // namespace std

/*
 * Computation methods of keys for searching UTXO in maps or in DB: hash ^ index
 */
uint256 HashXORIndex(const uint256& hash, uint32_t index); 

/**
 * UTXO stands for unspend transaction output
 */
class UTXO {
public:
    UTXO(const TxOutput& output, uint32_t index) : output_(output), index_(index) {}
    UTXO(const UTXO&) = default;
    UTXO(VStream& s) {
        s >> *this; 
    }

    ADD_SERIALIZE_METHODS;
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(output_);
    }

    friend std::string std::to_string(const UTXO&);

    bool operator==(const UTXO& another) const {
        return index_ == another.index_ && output_ == another.output_;
    }

    bool operator!=(const UTXO& another) const {
        return !(*this == another);
    }

    /*
     * Key for searching in maps or in DB: hash ^ index
     * This methods is called immediately after the tx containing this output is validated,
     * and this utxo is then stored in a map in Chain
     */
    uint256 GetKey() {
        return HashXORIndex(output_.GetParentTx()->GetParentBlock()->GetHash(), index_);
    }

    uint64_t HashCode() const {
        return std::hash<uint256>()(output_.GetParentTx()->GetParentBlock()->GetHash()) ^ index_;
    }

private:
    TxOutput output_;
    uint32_t index_;
};

template <>
struct std::hash<UTXO> {
    size_t operator()(const UTXO& u) const {
        return u.HashCode();
    }
};

/**
 * TXOC stands for a delta of transaction output changes
 */
class TXOC {
public:
    TXOC() : created_(), spent_() {}
    TXOC(std::vector<UTXO> created, std::vector<uint256> spent) : created_(created), spent_(spent) {}

    void AddToCreated(UTXO&& utxo);
    void AddToCreated(const TxOutput& output, uint32_t index);
    void AddToSpent(const TxInput& input);

    const std::vector<UTXO>& GetTxOutsCreated() const {
        return created_;
    }

    const std::vector<uint256>& GetTxOutsSpent() const {
        return spent_;
    }

    friend std::string std::to_string(const TXOC&);

private:
    std::vector<UTXO> created_;
    std::vector<uint256> spent_;
};

#endif /* ifndef __SRC_UTXO_H__ */
