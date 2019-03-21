#ifndef __SRC_TRANSACTION_H__
#define __SRC_TRANSACTION_H__

#include <uint256.h>
#include <script/script.h>
#include <coin.h>

/**
 * Outpoint of a transaction, which points to an input in a previous block
 */
class TxOutPoint {
    public:
        uint256 hash;
        uint32_t index;

        TxOutPoint(): index((uint32_t) -1) {}
        TxOutPoint(const uint256& fromBlock, uint32_t index): hash(fromBlock), index(index) {}

        // TODO: add serialization method
        //SERIALIZATION;

        template <typename Stream, typename Operation>
            inline void Serialize(Stream& s, Operation action) {
                // TODO: add readwrite method
                //READWRITE(hash);
                //READWRITE(index);
            }
};

class TxInput {
    public:
        TxOutPoint outpoint;
        Script scriptSig;

        TxInput();
        explicit TxInput(TxOutPoint outpoint, Script scriptSig=Script());
        TxInput(uint256 fromBlock, uint32_t index, Script scriptSig=Script());

        // TODO: add serialization method
        //SERIALIZATION;

        template <typename Stream, typename Operation>
            inline void Serialize(Stream& s, Operation action) {
                // TODO: add readwrite method
                //READWRITE(outpoint);
                //READWRITE(scriptSig);
            }
};

class TxOutput {
    public:
        // TODO: implement Coin class
        Coin value;
        Script scriptPubKey;

        TxOutput()
        {
            value = -1;
            scriptPubKey.clear();
        }

        TxOutput(const Coin& value, Script scriptPubKey);

        // TODO: add serialization method
        //SERIALIZATION;

        template <typename Stream, typename Operation>
            inline void Serialize(Stream& s, Operation action) {
                // TODO: add readwrite method
                //READWRITE(value);
                //READWRITE(scriptPubKey);
            }
};

class Transaction {
    public:
        std::vector<TxInput> inputs;
        std::vector<TxOutput> outputs;
        const uint32_t version;
        bool isValid = false;

    private:
        const uint256 hash;
        bool isRegistration;
        Coin fee;

    public:
        Transaction();
        explicit Transaction(const Transaction& tx);
        Transaction(std::vector<TxInput>& inputs, std::vector<TxOutput>& outputs);
        Transaction(uint32_t version);

        // TODO: add serialization methods

        void AddInput(TxInput& input);
        void AddOutput(TxOutput& output);
        uint256 GetHash() const;
        bool IsRegistration() const
        {
            return isRegistration;
        }
};

#endif //__SRC_TRANSACTION_H__
