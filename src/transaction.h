#ifndef __SRC_TRANSACTION_H__
#define __SRC_TRANSACTION_H__

#include "block.h"
#include "coin.h"
#include "script/script.h"
#include "uint256.h"

/**
 * Outpoint of a transaction, which points to an input in a previous block
 */
class TxOutPoint {
    public:
        TxOutPoint(): index((uint32_t) -1) {}

        //TODO: search for the pointer of BlockIndex in Cat
        TxOutPoint(const uint256 fromBlock, uint32_t index): hash(fromBlock), index(index) {}

        std::string ToString() const;

        template<std::size_t N>
        decltype(auto) get() const {
            if constexpr (N == 0) return hash;
            else if constexpr (N == 1) return index;
        }

    private:
        struct BlockIndex* block;
        uint256 hash;
        uint32_t index;
};

class TxInput {
    public:
        TxOutPoint outpoint;
        Script scriptSig;

        TxInput();
        explicit TxInput(TxOutPoint outpoint, Script scriptSig=Script());
        TxInput(uint256 fromBlock, uint32_t index, Script scriptSig=Script());

        template<std::size_t N>
        decltype(auto) get() const {
            if constexpr (N == 0) return outpoint;
            else if constexpr (N == 1) return scriptSig;
        }

        bool IsRegistration() { return isRegistration; }
        std::string ToString() const;

    private:
        bool isRegistration;
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

        TxOutput(const Coin value, Script scriptPubKey);

        template<std::size_t N>
        decltype(auto) get() const {
            if constexpr (N == 0) return value;
            else if constexpr (N == 1) return scriptPubKey;
        }

        std::string ToString() const;
};

class Transaction {
    public:
        std::vector<TxInput> inputs;
        std::vector<TxOutput> outputs;
        uint32_t version;
        bool isValid = false;

    private:
        uint256 hash;
        bool isRegistration;
        Coin fee;

    public:
        Transaction();
        explicit Transaction(const Transaction& tx);
        Transaction(const std::vector<TxInput>& inputs, const std::vector<TxOutput>& outputs);
        Transaction(uint32_t version);

        void AddInput(TxInput& input);
        void AddOutput(TxOutput& output);
        uint256 GetHash() const;
        bool IsRegistration() const { return isRegistration; }

        std::string ToString() const;

        template<std::size_t N>
        decltype(auto) get() const {
            if constexpr (N == 0) return inputs;
            else if constexpr (N == 1) return outputs;
            else if constexpr (N == 2) return version;
            else if constexpr (N == 3) return isValid;
            else if constexpr (N == 4) return hash;
            else if constexpr (N == 5) return isRegistration;
            else if constexpr (N == 6) return fee;
        }
};

namespace std {
    // TxOutPoint 
    template<>
    struct tuple_size<TxOutPoint> : std::integral_constant<std::size_t, 2> {};
  
    template<std::size_t N>
    struct tuple_element<N, TxOutPoint> {
        using type = decltype(std::declval<TxOutPoint>().get<N>());
    };
  
    // TxInput 
    template<>
    struct tuple_size<TxInput> : std::integral_constant<std::size_t, 2> {};
  
    template<std::size_t N>
    struct tuple_element<N, TxInput> {
        using type = decltype(std::declval<TxInput>().get<N>());
    };
  
    // TxOutput 
    template<>
    struct tuple_size<TxOutput> : std::integral_constant<std::size_t, 2> {};
  
    template<std::size_t N>
    struct tuple_element<N, TxOutput> {
        using type = decltype(std::declval<TxOutput>().get<N>());
    };
  
    // Transaction 
    template<>
    struct tuple_size<Transaction> : std::integral_constant<std::size_t, 7> {};
  
    template<std::size_t N>
    struct tuple_element<N, Transaction> {
        using type = decltype(std::declval<Transaction>().get<N>());
    };
}

#endif //__SRC_TRANSACTION_H__
