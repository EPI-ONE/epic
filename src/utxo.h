#ifndef __SRC_UTXO_H__
#define __SRC_UTXO_H__

#include "block.h"

#include <unordered_set>

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

    const TxOutput& GetOutput() const {
        return output_;
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

typedef std::shared_ptr<const UTXO> UTXOPtr;

/**
 * TXOC stands for a delta of transaction output changes
 */
class TXOC {
public:
    TXOC() = default;
    TXOC(std::unordered_set<uint256>&& created, std::unordered_set<uint256>&& spent)
        : created_(std::move(created)), spent_(std::move(spent)) {}

    void AddToCreated(const UTXOPtr&);
    void AddToCreated(const uint256&, uint32_t);
    void AddToSpent(const TxInput&);
    void Merge(TXOC&&);

    const std::unordered_set<uint256>& GetTxOutsCreated() const {
        return created_;
    }
    const std::unordered_set<uint256>& GetTxOutsSpent() const {
        return spent_;
    }

    friend std::string std::to_string(const TXOC&);

private:
    // vectors representing keys of created and spent UTXO of encoding by the special XOR function respectively
    std::unordered_set<uint256> created_;
    std::unordered_set<uint256> spent_;
};

class ChainLedger {
public:
    ChainLedger()                   = default;
    ChainLedger(const ChainLedger&) = default;

    ChainLedger(std::unordered_map<uint256, UTXOPtr>&& pending,
        std::unordered_map<uint256, UTXOPtr>&& comfirmed,
        std::unordered_map<uint256, UTXOPtr>&& removed)
        : pending_(std::move(pending)), comfirmed_(std::move(comfirmed)), removed_(std::move(removed)) {}

    void AddToPending(UTXOPtr);
    UTXOPtr GetFromPending(const uint256&);
    UTXOPtr FindSpendable(const uint256&);
    void Update(const TXOC&);
    void Rollback(const TXOC&);

private:
    std::unordered_map<uint256, UTXOPtr> pending_;
    std::unordered_map<uint256, UTXOPtr> comfirmed_;
    std::unordered_map<uint256, UTXOPtr> removed_;
};

#endif /* ifndef __SRC_UTXO_H__ */
