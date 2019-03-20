#ifndef __SRC_MILESTONE_H__
#define __SRC_MILESTONE_H__

#include <vector>
#include <unordered_map>
#include <utility>
#include <utils/uint256.h>
#include <utils/stdint.h>

class Block;
class TxOutput;

class Milestone {
    private:
        size_t height_;

        std::shared_ptr<Milestone> pprevious_;
        std::shared_ptr<Milestone> pnext_;

        // a vector consists of blocks with its offset w.r.t this level set of
        // this milestone
        std::vector<std::pair<Block, int> > vblockstore_;

        uint32_t nbits_milestone_target_;
        uint32_t nbits_block_target_;

        std::unordered_map<uint256, uint256> pubkey_snapshot_;

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
