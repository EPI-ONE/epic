#ifndef __SRC_BLOCK_H__
#define __SRC_BLOCK_H__

#include <cstdint>

#include "serialize.h"
#include "transaction.h"
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
    Block()             = default;
    Block(const Block&) = default;

    // Initialize with all the header fields
    Block(uint32_t version,
        uint256 milestoneHash,
        uint256 prevBlockHash,
        uint256 tipBlockHash,
        uint256 contentHash,
        uint32_t time,
        uint32_t difficultyTarget,
        uint32_t nonce)
        : nVersion_(version), hashMilestoneBlock_(milestoneHash),
          hashPrevBlock_(prevBlockHash), hashTipBlock_(tipBlockHash),
          hashTransaction_(contentHash), nTime_(time), nBits_(difficultyTarget),
          nNonce_(nonce) {}
    ~Block(){};

    // daily utils
    // solve the block maht puzzle
    void Solve();
    // verify block content syntactically
    bool Verify();
    void AddTransaction(const Transaction& t);

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(nVersion_);
        READWRITE(hashMilestoneBlock_);
        READWRITE(hashPrevBlock_);
        READWRITE(hashTipBlock_);
        // READWRITE(?); content hash
        READWRITE(nTime_);
        READWRITE(nBits_);
        READWRITE(nNonce_);
        READWRITE(tx_);
    }

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
    std::vector<Transaction> tx_;
};

struct BlockIndex {
    // Only one of ptr_block and file_descriptor (together with offset)
    // should be assigned a value
    Block* ptr_block;
    int file_descriptor;
    uint32_t offset; // offset in file
};

#endif
