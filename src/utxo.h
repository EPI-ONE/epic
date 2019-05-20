#ifndef __SRC_UTXO_H__
#define __SRC_UTXO_H__

#include "block.h"

class UTXO;
class TXOC;
namespace std {
string to_string(const UTXO&);
string to_string(const TXOC&);
} // namespace std

/**
 * Computation methods of keys for searching UTXO in maps or in DB: hash ^ index
 */
uint256 XOR(const uint256& hash, uint32_t index);

/**
 * UTXO stands for unspend transaction output
 */
class UTXO {
public:
    UTXO(const TxOutput& output, uint32_t index) : output_(output), index_(index) {}
    UTXO(const UTXO&) = default;
    UTXO(VStream& s) : index_(-1) {
        s >> output_;
    }

    ADD_SERIALIZE_METHODS;
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(output_);
    }

    bool operator==(const UTXO& another) const {
        return index_ == another.index_ && output_ == another.output_;
    }

    bool operator!=(const UTXO& another) const {
        return !(*this == another);
    }

    uint256 GetContainingBlkHash() const;

    /**
     * Key for searching in maps or in DB: hash ^ index
     * This methods is called immediately after the tx containing this output is validated,
     * and this utxo is then stored in a map in Chain
     */
    uint256 GetKey() const;

    uint64_t HashCode() const;

    friend std::string std::to_string(const UTXO&);

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
    TXOC(std::vector<UTXO> created, std::vector<uint256> spent)
        : created_(std::move(created)), spent_(std::move(spent)) {}

    void AddToCreated(UTXO&&);
    void AddToCreated(const TxOutput&, uint32_t);
    void AddToSpent(const UTXO&);
    void AddToSpent(const TxInput&);

    const std::vector<UTXO>& GetTxOutsCreated() const {
        return created_;
    }
    const std::vector<uint256>& GetTxOutsSpent() const {
        return spent_;
    }

    friend std::string std::to_string(const TXOC&);

private:
    std::vector<UTXO> created_;
    // a vector representing keys of spent UTXO of encoding by XOR function in this file
    std::vector<uint256> spent_;
};

#endif /* ifndef __SRC_UTXO_H__ */
