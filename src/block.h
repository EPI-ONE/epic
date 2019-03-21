#ifndef __SRC_BLOCK_H__
#define __SRC_BLOCK_H__

#include <stdint.h>
#include <uint256.h>
#include <transaction.h>

class Block {
    public:
        // parameter restrictions

        // constructor and destructor
        Block()=default;
        Block(const Block&) = default;
        ~Block() {};

        // daily utils
        // solve the block maht puzzle
        void Solve();
        // verify block content syntactically
        bool Verify();
        void AddTransaction(const Transaction& t);

        // get & set methods

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

#endif
