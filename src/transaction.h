#ifndef __SRC_TRANSACTION_H__
#define __SRC_TRANSACTION_H__

#include <sstream>
#include <unordered_set>

#include "caterpillar.h"
#include "coin.h"
#include "hash.h"
#include "script.h"
#include "serialize.h"
#include "uint256.h"

static const uint32_t NEGATIVE_ONE = 0xFFFFFFFF;

class Block;
class Transaction;

class TxOutPoint {
public:
    uint256 bHash;
    uint32_t index;

    TxOutPoint() : index((uint32_t) -1) {}

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

    bool IsRegistration() const {
        return (outpoint.index & NEGATIVE_ONE) == NEGATIVE_ONE;
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
    };

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
        value = NEGATIVE_ONE;
        scriptPubKey.clear();
    }

    TxOutput(const Coin& value, const Script& scriptPubKey);

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
        VALID   = 1,
        INVALID = 2,
    };

    Transaction() {
        inputs  = std::vector<TxInput>();
        outputs = std::vector<TxOutput>();
        status_ = UNKNOWN;
    };
    explicit Transaction(const Transaction& tx);

    void AddInput(TxInput&& input);
    void AddOutput(TxOutput&& output);

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

    uint256& GetHash() {
        if (!hash_.IsNull()) {
            return hash_;
        }
        hash_ = Hash<1>(VStream(*this));
        return hash_;
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
    };

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
