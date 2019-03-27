#ifndef __SRC_BLOCK_H__
#define __SRC_BLOCK_H__

#include <cstdint>

#include "coin.h"
#include "milestone.h"
#include "uint256.h"

class Transaction;

static constexpr int HEADER_SIZE = 144;
static constexpr uint32_t ALLOWED_TIME_DRIFT = 2 * 60 * 60;
static constexpr uint32_t MAX_BLOCK_SIZE = 20 * 1000;
static constexpr uint32_t GENESIS_BLOCK_HEIGHT = 0;
static constexpr uint32_t GENESIS_BLOCK_VERSION = 1;

class Block {
    public:
        // parameter restrictions

        // constructor and destructor
        Block()=default;

        Block(const Block&) = default;
        Block(uint32_t version);

        // Initialize with all the header fields
        Block(uint32_t version, uint256 milestoneHash, uint256 prevBlockHash,
              uint256 tipBlockHash, uint256 contentHash, uint32_t time,
              uint32_t difficultyTarget, uint32_t nonce) :
            version_(version), milestoneBlockHash_(milestoneHash),
            prevBlockHash_(prevBlockHash), tipBlockHash_(tipBlockHash),
            contentHash_(contentHash), time_(time),
            diffTarget_(difficultyTarget), nonce_(nonce) {}

        ~Block(){};

        // daily utils
        // solve the block maht puzzle
        void Solve();
        // verify block content syntactically
        bool Verify();
        void AddTransaction(const Transaction& t);

        // get & set methods

    private:
        // header
        uint32_t version_;
        uint256 milestoneBlockHash_;
        uint256 prevBlockHash_;
        uint256 tipBlockHash_;
        uint256 contentHash_;
        uint32_t time_;
        uint32_t diffTarget_;
        uint32_t nonce_;

        // info to be stored in db
        Coin cumulativeReward_;
        uint64_t minerChainHeight_;
        Milestone milestoneInstance_;
        bool isMilestone_;

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

#endif
