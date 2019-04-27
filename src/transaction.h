#ifndef __SRC_TRANSACTION_H__
#define __SRC_TRANSACTION_H__

#include "coin.h"
#include "hash.h"
#include "script.h"
#include "serialize.h"
#include "tinyformat.h"

#include <limits>
#include <sstream>
#include <unordered_set>

static const uint32_t UNCONNECTED = UINT_LEAST32_MAX;

class Block;
class Transaction;

class TxOutPoint {
public:
    uint256 bHash;
    uint32_t index;

    TxOutPoint() : index(UNCONNECTED) {}

    // TODO: search for the pointer of BlockIndex in Cat
    TxOutPoint(const uint256 fromBlock, const uint32_t index) : bHash(fromBlock), index(index) {}

    ADD_SERIALIZE_METHODS;
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(bHash);
        READWRITE(index);
    }

    friend bool operator==(const TxOutPoint& out1, const TxOutPoint& out2) {
        return out1.index == out2.index && out1.bHash == out2.bHash;
    }
};

/** Key hasher for unordered_set */
template <>
struct std::hash<TxOutPoint> {
    size_t operator()(const TxOutPoint& x) const {
        return std::hash<uint256>()(x.bHash) + (size_t) index;
    }
};

class TxInput {
public:
    TxOutPoint outpoint;
    Script scriptSig;

    TxInput(){};
    explicit TxInput(const TxOutPoint& outpoint, const Script& scriptSig = Script());
    TxInput(const uint256& fromBlock, const uint32_t index, const Script& scriptSig = Script());
    TxInput(const Script& script);

    friend bool operator==(const TxInput& a, const TxInput& b) {
        return (a.outpoint == b.outpoint) && (a.scriptSig.bytes == b.scriptSig.bytes);
    }

    bool IsRegistration() const {
        return outpoint.index == UNCONNECTED;
    }
    bool IsFirstRegistration() const {
        return outpoint.bHash == Hash::ZERO_HASH && IsRegistration();
    }

    ADD_SERIALIZE_METHODS;
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(outpoint);
        READWRITE(scriptSig);
    }

    void SetParent(const Transaction* const tx) {
        parentTx_ = tx;
    }

    const Transaction* GetParentTx() {
        return parentTx_;
    }

private:
    const Transaction* parentTx_;
};

class TxOutput {
public:
    // TODO: implement Coin class
    Coin value;
    Script scriptPubKey;

    TxOutput() {
        value = IMPOSSIBLE_COIN;
        scriptPubKey.clear();
    }

    TxOutput(const Coin& value, const Script& scriptPubKey);

    friend bool operator==(const TxOutput& a, const TxOutput& b) {
        return (a.value == b.value) && (a.scriptPubKey.bytes == b.scriptPubKey.bytes);
    }

    ADD_SERIALIZE_METHODS;
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(value);
        READWRITE(scriptPubKey);
    }

    void SetParent(const Transaction* const tx) {
        parentTx_ = tx;
    }

private:
    const Transaction* parentTx_;
};

class Transaction {
public:
    enum Validity : uint8_t {
        UNKNOWN = 0,
        VALID,
        INVALID,
    };

    Transaction(Validity&& status = UNKNOWN) {
        inputs  = std::vector<TxInput>();
        outputs = std::vector<TxOutput>();
        status_ = status;
    }

    explicit Transaction(const Transaction& tx);

    Transaction& AddInput(TxInput&& input);
    Transaction& AddOutput(TxOutput&& output);

    void FinalizeHash() {
        if (!hash_.IsNull())
            hash_ = Hash<1>(VStream(*this));
    }

    const TxInput& GetInput(size_t index) const {
        return inputs[index];
    }

    const TxOutput& GetOutput(size_t index) const {
        return outputs[index];
    }

    const std::vector<TxInput>& GetInputs() const {
        return inputs;
    }

    const std::vector<TxOutput>& GetOutputs() const {
        return outputs;
    }

    const uint256& GetHash() const {
        return hash_;
    }

    friend bool operator==(const Transaction& a, const Transaction& b) {
        return a.GetHash() == b.GetHash();
    }

    bool IsRegistration() const {
        return inputs.size() == 1 && inputs.front().IsRegistration();
    }

    bool IsFirstRegistration() const {
        return inputs.size() == 1 && inputs.front().IsFirstRegistration() && outputs.front().value == ZERO_COIN;
    }

    bool Verify() const;

    void Validate() {
        status_ = VALID;
    }

    void Invalidate() {
        status_ = INVALID;
    }

    void SetStatus(Validity&& status) {
        status_ = status;
    }

    Validity GetStatus() const {
        return status_;
    }

    ADD_SERIALIZE_METHODS;
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(inputs);
        READWRITE(outputs);
    }

    void SetParent(const Block* const blk) {
        parentBlock_ = blk;
    }

private:
    std::vector<TxInput> inputs;
    std::vector<TxOutput> outputs;

    uint256 hash_;
    Coin fee_;
    Validity status_;

    const Block* parentBlock_;
};

namespace std {
string to_string(const TxOutPoint& outpoint);
string to_string(const TxInput& input);
string to_string(const TxOutput& output);
string to_string(Transaction& tx);
} // namespace std

#endif //__SRC_TRANSACTION_H__
