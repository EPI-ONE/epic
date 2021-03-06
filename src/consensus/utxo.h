// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef EPIC_UTXO_H
#define EPIC_UTXO_H

#include "block.h"
#include "increment.h"

#include <unordered_set>

class UTXO;
class TXOC;
class ChainLedger;

namespace std {
string to_string(const UTXO&);
string to_string(const TXOC&);
string to_string(const ChainLedger&);
} // namespace std

/**
 * UTXO stands for unspend transaction output
 */
class UTXO {
public:
    UTXO(const TxOutput& output, uint32_t txIdx, uint32_t outIdx)
        : output_(output), txIndex_(txIdx), outIndex_(outIdx) {}

    UTXO(const UTXO&) = default;

    explicit UTXO(VStream& s) : txIndex_(-1), outIndex_(-1) {
        s >> output_;
    }

    ADD_SERIALIZE_METHODS
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

    std::pair<uint32_t, uint32_t> GetIndices() const {
        return std::make_pair(txIndex_, outIndex_);
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
    uint32_t txIndex_;
    uint32_t outIndex_;
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
    void AddToCreated(const uint256&, uint32_t, uint32_t);
    void AddToSpent(const TxInput&);
    void Merge(TXOC txoc);

    bool Empty();

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
    UTXOPtr FindFromLedger(const uint256&); // for created and spent UTXOs
    UTXOPtr FindSpendable(const uint256&);
    UTXOPtr GetFromPending(const uint256&);
    void Invalidate(const TXOC&);
    void Update(const TXOC&);
    void Remove(const TXOC&);
    void Rollback(const TXOC&);

    bool IsSpendable(const uint256&) const;

private:
    std::unordered_map<uint256, UTXOPtr> pending_;
    std::unordered_map<uint256, UTXOPtr> confirmed_;
    std::unordered_map<uint256, UTXOPtr> removed_;

    friend std::string std::to_string(const ChainLedger&);
};

TXOC CreateTXOCFromInvalid(const Transaction&, uint32_t txIndex);
#endif // EPIC_UTXO_H
