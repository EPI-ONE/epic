#ifndef __SRC_TRANSACTION_H__
#define __SRC_TRANSACTION_H__

#include "caterpillar.h"
#include "coin.h"
#include "hash.h"
#include "script.h"
#include "serialize.h"
#include "uint256.h"
#include <sstream>

static const uint32_t NEGATIVE_ONE = 0xFFFFFFFF;

class Block;
class Transaction;
class TxOutPoint {
public:
    uint256 hash;
    uint32_t index;

    TxOutPoint() : index((uint32_t) -1) {}

    // TODO: search for the pointer of BlockIndex in Cat
    TxOutPoint(const uint256 fromBlock, const uint32_t index) : hash(fromBlock), index(index) {}

    ADD_SERIALIZE_METHODS;
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(hash);
        READWRITE(index);
    }
    std::string ToString() const;

private:
    struct BlockIndex* block;
};

class TxInput {
public:
    TxOutPoint outpoint;
    Script scriptSig;

    TxInput(){};
    explicit TxInput(const TxOutPoint& outpoint, const Script& scriptSig = Script());
    TxInput(const uint256& fromBlock, const uint32_t index, const Script& scriptSig = Script());
    TxInput(const Script& script);

    ADD_SERIALIZE_METHODS;
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(outpoint);
        READWRITE(scriptSig);
    }

    bool IsRegistration() const {
        return (outpoint.index & NEGATIVE_ONE) == NEGATIVE_ONE;
    }
    bool IsFirstRegistration() const {
        return outpoint.hash == Hash::ZERO_HASH && IsRegistration();
    }
    std::string ToString() const;
    void SetParent(const Transaction& tx) {
        parentTx_ = std::make_shared<Transaction>(tx);
    };

private:
    std::shared_ptr<Transaction> parentTx_;
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
    void SetParent(const Transaction& tx) {
        parentTx_ = std::make_shared<Transaction>(tx);
    }

    ADD_SERIALIZE_METHODS;
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(value);
        READWRITE(scriptPubKey);
    }

    std::string ToString() const;

private:
    std::shared_ptr<Transaction> parentTx_;
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

    ADD_SERIALIZE_METHODS;
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(inputs);
        READWRITE(outputs);
    }

    void AddInput(TxInput&& input);
    void AddOutput(TxOutput&& output);
    void SetParent(const Block& blk) {
        parentBlock_ = std::make_shared<Block>(blk);
    };

    // TODO:
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
        return inputs.front().IsFirstRegistration() && outputs.front().value == ZERO_COIN;
    }
    // TODO:
    bool Verify() const {
        return true;
    }
    void Validate() {
        status_ = VALID;
    }
    void Invalidate() {
        status_ = INVALID;
    }
    Validity GetStatus() const {
        return status_;
    }
    void SetStatus(enum Validity s) {
        status_ = s;
    }
    std::string ToString();

private:
    std::vector<TxInput> inputs;
    std::vector<TxOutput> outputs;

    uint256 hash_;
    Coin fee_;
    std::shared_ptr<Block> parentBlock_;
    Validity status_;
};

#endif //__SRC_TRANSACTION_H__
