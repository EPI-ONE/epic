#ifndef __SRC_BLOCK_H__
#define __SRC_BLOCK_H__

#include <cstdint>

#include "transaction.h"
#include "uint256.h"

class Transaction;

class Block {
    public:
        // parameter restrictions

        // constructor and destructor
        Block()=default;
        Block(const Block&) = default;

        // Initialize with all the header fields
        Block(uint32_t version, uint256 milestoneHash, uint256 prevBlockHash,
              uint256 tipBlockHash, uint256 contentHash, uint32_t time,
              uint32_t difficultyTarget, uint32_t nonce)
            : nVersion_(version), hashMilestoneBlock_(milestoneHash),
              hashPrevBlock_(prevBlockHash), hashTipBlock_(tipBlockHash),
              hashTransaction_(contentHash), nTime_(time),
              nBits_(difficultyTarget), nNonce_(nonce) {}
        ~Block(){};

        // daily utils
        // solve the block maht puzzle
        void Solve();
        // verify block content syntactically
        bool Verify();
        void AddTransaction(const Transaction& t);

        // get & set methods
        template<std::size_t N>
        decltype(auto) get() const {
            if constexpr (N == 0) return nVersion_;
            else if constexpr (N == 1) return hashMilestoneBlock_;
            else if constexpr (N == 2) return hashPrevBlock_;
            else if constexpr (N == 3) return hashTipBlock_;
            else if constexpr (N == 4) return hashTransaction_;
            else if constexpr (N == 5) return nTime_;
            else if constexpr (N == 6) return nBits_;
            else if constexpr (N == 7) return nNonce_;
            else if constexpr (N == 8) return ptx_;
        }


    private:
        // header
        int32_t nVersion_;
        uint256 hashMilestoneBlock_;
        uint256 hashPrevBlock_;
        uint256 hashTipBlock_;
        uint256 hashTransaction_;
        uint32_t nTime_;
        uint32_t nBits_; // difficultyTarget
        uint32_t nNonce_;

        // content
        std::shared_ptr<Transaction> ptx_;
};

struct BlockIndex {
    // Only one of ptr_block and file_descriptor (together with offset)
    // should be assigned a value
    Block* ptr_block;
    int file_descriptor;
    uint32_t offset; // offset in file
};

namespace std {
    // Block 
    template<>
    struct tuple_size<Block> : std::integral_constant<std::size_t, 9> {};
  
    template<std::size_t N>
    struct tuple_element<N, Block> {
        using type = decltype(std::declval<Block>().get<N>());
    };
}

#endif
