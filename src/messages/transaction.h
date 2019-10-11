// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef EPIC_TRANSACTION_H
#define EPIC_TRANSACTION_H

#include "net_message.h"
#include "tasm.h"
#include "tinyformat.h"

#include <cassert>

static const uint32_t UNCONNECTED = UINT_LEAST32_MAX;

class Block;
class Transaction;

/**
 * Computation methods of keys for searching UTXO in maps or in DB: hash ^ txIdx ^ outIdx
 */
uint256 ComputeUTXOKey(const uint256& hash, uint32_t txIdx, uint32_t outIdx);

class TxOutPoint {
public:
    uint256 bHash;
    uint32_t txIndex;
    uint32_t outIndex;

    TxOutPoint() : bHash(Hash::GetZeroHash()), txIndex(UNCONNECTED), outIndex(UNCONNECTED) {}
    TxOutPoint(const uint256& fromBlock, uint32_t index1, uint32_t index2)
        : bHash(fromBlock), txIndex(index1), outIndex(index2) {}

    friend bool operator==(const TxOutPoint& out1, const TxOutPoint& out2) {
        return out1.txIndex == out2.txIndex && out1.outIndex == out2.outIndex && out1.bHash == out2.bHash;
    }

    ADD_SERIALIZE_METHODS
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(bHash);
        READWRITE(txIndex);
        READWRITE(outIndex);
    }

    uint64_t HashCode() const {
        return txIndex ^ outIndex ^ bHash.GetCheapHash();
    }

    uint256 GetOutKey() const {
        return ComputeUTXOKey(bHash, txIndex, outIndex);
    }
};

// Key hashers for unordered_set
template <>
struct std::hash<TxOutPoint> {
    size_t operator()(const TxOutPoint& x) const {
        return x.HashCode();
    }
};

class TxInput {
public:
    TxOutPoint outpoint;
    Tasm::Listing listingContent;

    TxInput() = default;
    TxInput(const TxOutPoint& outpointToprev, Tasm::Listing listing)
        : outpoint(outpointToprev), listingContent(std::move(listing)) {}
    TxInput(const uint256& fromBlock, uint32_t txIdx, uint32_t outIdx, Tasm::Listing listing)
        : outpoint(fromBlock, txIdx, outIdx), listingContent(std::move(listing)) {}

    TxInput(const TxOutPoint& outpoint,
            const CPubKey& pubkey,
            const uint256& hashMsg,
            const std::vector<unsigned char>& sig)
        : TxInput(outpoint, Tasm::Listing{VStream(pubkey, sig, hashMsg)}) {}

    explicit TxInput(Tasm::Listing listing) : outpoint(), listingContent(std::move(listing)) {}

    bool IsRegistration() const;

    bool IsFirstRegistration() const;

    void SetParent(const Transaction* const tx) const;

    const Transaction* GetParentTx() const;

    friend bool operator==(const TxInput& a, const TxInput& b) {
        return (a.outpoint == b.outpoint) && (a.listingContent == b.listingContent);
    }

    ADD_SERIALIZE_METHODS
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(outpoint);
        READWRITE(listingContent);
    }

private:
    mutable const Transaction* parentTx_ = nullptr;
};

class TxOutput {
public:
    Coin value;
    Tasm::Listing listingContent;

    TxOutput();

    TxOutput(const Coin& value, const Tasm::Listing& ListingData);

    TxOutput(const uint64_t& coinValue, const Tasm::Listing& listingData);

    void SetParent(const Transaction* const tx) const;

    const Transaction* GetParentTx() const;

    friend bool operator==(const TxOutput& a, const TxOutput& b) {
        return (a.value == b.value) && (a.listingContent == b.listingContent);
    }

    ADD_SERIALIZE_METHODS
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(value);
        READWRITE(listingContent);
    }

private:
    mutable const Transaction* parentTx_ = nullptr;
};

class Transaction : public NetMessage {
public:
    /**
     * constructor of an empty transcation
     */
    Transaction() = default;

    /**
     * copy and move constructor with computing hash and setting parent block
     */
    Transaction(const Transaction& tx);
    Transaction(Transaction&&) noexcept;

    /**
     * constructor of first registration where $addr is the address to redeem in the future
     */
    explicit Transaction(const CKeyID& addr);

    Transaction(std::vector<TxInput> inputs, std::vector<TxOutput> outputs)
        : inputs_(std::move(inputs)), outputs_(std::move(outputs)) {
        SetParents();
        FinalizeHash();
    }

    template <typename Stream>
    explicit Transaction(Stream& vs) {
        ::Deserialize(vs, *this);
    }

    Transaction& operator=(const Transaction&) = default;
    Transaction& operator=(Transaction&&) = default;

    bool IsNull() const {
        return inputs_.empty() && outputs_.empty();
    }

    void SetParents() const;

    Transaction& AddInput(TxInput&& input);
    Transaction& AddOutput(TxOutput&& output);
    Transaction& AddOutput(uint64_t, const CKeyID&);
    Transaction& AddOutput(const Coin&, const CKeyID&);

    void FinalizeHash();
    bool Verify() const;

    const std::vector<TxInput>& GetInputs() const;
    std::vector<TxInput>& GetInputs();
    const std::vector<TxOutput>& GetOutputs() const;
    std::vector<TxOutput>& GetOutputs();
    const uint256& GetHash() const;

    bool IsRegistration() const;
    bool IsFirstRegistration() const;

    void SetParent(const Block* const blk) const;
    const Block* GetParentBlock() const;

    uint64_t HashCode() const;

    ADD_SERIALIZE_METHODS
    ADD_NET_SERIALIZE_METHODS
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(inputs_);
        READWRITE(outputs_);
        if (ser_action.ForRead()) {
            FinalizeHash();
            SetParents();
        }
    }

    friend bool operator==(const Transaction& a, const Transaction& b) {
        return a.GetHash() == b.GetHash();
    }

private:
    std::vector<TxInput> inputs_;
    std::vector<TxOutput> outputs_;

    uint256 hash_;
    mutable const Block* parentBlock_ = nullptr;
};

typedef std::shared_ptr<const Transaction> ConstTxPtr;

// Key hashers for unordered_set
template <>
struct std::hash<Transaction> {
    size_t operator()(const Transaction& x) const {
        return x.HashCode();
    }
};

bool VerifyInOut(const TxInput&, const Tasm::Listing&);

namespace std {
string to_string(const TxOutPoint& outpoint);
string to_string(const TxInput& input);
string to_string(const TxOutput& output);
string to_string(const Transaction& tx);
} // namespace std

#endif // EPIC_TRANSACTION_H
