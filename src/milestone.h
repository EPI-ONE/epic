#ifndef __SRC_MILESTONE_H__
#define __SRC_MILESTONE_H__

#include <vector>
#include <cstdint>
#include <unordered_map>
#include <utility>

#include "uint256.h"

class Block;
class TxOutput;

class Milestone {
    private:
        size_t height_;

        std::shared_ptr<Milestone> pprevious_;
        std::shared_ptr<Milestone> pnext_;

        // a vector consists of blocks with its offset w.r.t this level set of
        // this milestone
        std::vector<std::pair<std::shared_ptr<Block>, uint64_t> > vblockstore_;

        uint32_t nbitsMilestoneTarget_;
        uint32_t nbitsBlockTarget_;

        std::unordered_map<uint256, uint256> pubkeySnapshot_;

    public:
        Milestone()=default;
        Milestone(const Milestone&)=default;
        ~Milestone() {};

        // difficulty calculation
        void CheckDifficultyTransitions();
        // when receiving a milestone block, return a new milestone object
        std::unique_ptr<Milestone> AddBlock(
                const std::shared_ptr<const Block> pblock,
                const std::shared_ptr<std::vector<const Block>> pvblockstore,
                const std::shared_ptr<std::unordered_map<uint256, TxOutput> > mpubkeys);

        // call Caterpillar to store all the blocks to file
        void WriteToFile();

        // get and set methods
};

#endif // __SRC_MILESTONE_H__
