#ifndef __SRC_MILESTONE_H__
#define __SRC_MILESTONE_H__

#include<unordered_map>
#include <utils/uint256.h>
#include <utils/stdint.h>

class Block;
class Levelset;
class TxOutput;

class Milestone {
    private:
        uint32_t height_;

        std::shared_ptr<Milestone> pprevious_;
        std::shared_ptr<Milestone> pnext_;
        std::unique_ptr<Levelset> plevelset_;

        std::shared_ptr<Block> pblock_;

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
        std::unique_ptr<Milestone> AddBlock(const std::shared_ptr<const Block> pblock,
                const std::shared_ptr<std::vector<const Block>> pvblockstore,
                const std::shared_ptr<std::unordered_map<uint256, TxOutput> > mpubkeys);

        // get and set methods
};

#endif // __SRC_MILESTONE_H__
