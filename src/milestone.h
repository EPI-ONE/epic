#ifndef __SRC_MILESTONE_H__
#define __SRC_MILESTONE_H__

#include <cstdint>
#include <unordered_map>
#include <utility>
#include <vector>

#include "block.h"

class Block;
class TxOutput;

class Milestone {
private:
    size_t height_;

    std::shared_ptr<Milestone> pprevious_;
    std::shared_ptr<Milestone> pnext_;

    // a vector consists of blocks with its offset w.r.t this level set of
    // this milestone
    std::vector<std::pair<std::shared_ptr<Block>, uint64_t>> vblockstore_;

    std::unordered_map<uint256, uint256> pubkeySnapshot_;

public:
    uint64_t height;
    arith_uint256 chainwork;
    time_t lastUpdateTime_;
    arith_uint256 milestoneTarget_;
    arith_uint256 blockTarget_;
    uint64_t hashRate_;

    Milestone()                 = default;
    Milestone(const Milestone&) = default;
    ~Milestone(){};

    // difficulty calculation
    void CheckDifficultyTransitions();

    // when receiving a milestone block, return a new milestone object
    std::unique_ptr<Milestone> AddBlock(const std::shared_ptr<const Block> pblock,
        const std::shared_ptr<std::vector<const Block>> pvblockstore,
        const std::shared_ptr<std::unordered_map<uint256, TxOutput>> mpubkeys);

    // call Caterpillar to store all the blocks to file
    void WriteToFile();

    template <typename Stream>
    void Serialize(Stream& s) const {
        ::Serialize(s, VARINT(height));
        ::Serialize(s, chainwork.GetCompact());
        ::Serialize(s, *((int64_t*) &lastUpdateTime_));
        ::Serialize(s, milestoneTarget_.GetCompact());
        ::Serialize(s, blockTarget_.GetCompact());
        ::Serialize(s, VARINT(hashRate_));
    }

    template <typename Stream>
    void Unserialize(Stream& s) {
        ::Unserialize(s, VARINT(height));
        chainwork.SetCompact(ser_readdata32(s));
        lastUpdateTime_ = ser_readdata64(s);
        milestoneTarget_.SetCompact(ser_readdata32(s));
        blockTarget_.SetCompact(ser_readdata32(s));
        ::Unserialize(s, VARINT(hashRate_));
    }
};

#endif // __SRC_MILESTONE_H__
