#ifndef __SRC_UTXO_H__
#define __SRC_UTXO_H__

#include "block.h"
#include "increment.h"

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
    explicit UTXO(VStream& s) : index_(-1) {
        s >> output_;
    }

    ADD_SERIALIZE_METHODS;
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(output_);
    }

    bool operator==(const UTXO& another) const {
        return output_ == another.output_;
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
 * TXOC stands for a delta of transaction output changes,
 * containing vectors representing keys of created and spent
 * UTXO of encoding by the special XOR function respectively
 */
class TXOC {
public:
    TXOC()            = default;
    TXOC(const TXOC&) = default;
    TXOC(TXOC&&)      = default;
    TXOC& operator=(const TXOC& other) = default;
    TXOC& operator=(TXOC&& other) = default;
    ~TXOC()                       = default;

    TXOC(std::unordered_set<uint256> created, std::unordered_set<uint256> spent)
        : increment_(std::move(created), std::move(spent)) {}

    void AddToCreated(const UTXOPtr&);
    void AddToCreated(const uint256&, uint32_t);
    void AddToSpent(const TxInput&);
    void Merge(TXOC txoc);

    const std::unordered_set<uint256>& GetSpent() const {
        return increment_.GetRemoved();
    }
    const std::unordered_set<uint256>& GetCreated() const {
        return increment_.GetCreated();
    }

    friend std::string std::to_string(const TXOC&);

private:
    Increment<uint256> increment_;
};

class ChainLedger {
public:
    ChainLedger()                   = default;
    ChainLedger(const ChainLedger&) = default;
    ~ChainLedger()                  = default;

    ChainLedger(std::unordered_map<uint256, UTXOPtr> pending,
                std::unordered_map<uint256, UTXOPtr> confirmed,
                std::unordered_map<uint256, UTXOPtr> removed)
        : pending_(std::move(pending)), confirmed_(std::move(confirmed)), removed_(std::move(removed)) {}

    void AddToPending(UTXOPtr);
    UTXOPtr FindFromLedger(const uint256&); // might be already spent
    UTXOPtr FindSpendable(const uint256&);
    UTXOPtr GetFromPending(const uint256&);
    void Invalidate(const TXOC&);
    void Update(const TXOC&);
    void Remove(const TXOC&);
    void Rollback(const TXOC&);

private:
    std::unordered_map<uint256, UTXOPtr> pending_;
    std::unordered_map<uint256, UTXOPtr> confirmed_;
    std::unordered_map<uint256, UTXOPtr> removed_;
};

TXOC CreateTXOCFromInvalid(const Block&);
#endif /* ifndef __SRC_UTXO_H__ */
