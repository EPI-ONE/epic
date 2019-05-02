#include "chain.h"

std::shared_ptr<Milestone> Chain::GetChainHead() const {
    return vmilestones_.front();
}

void Chain::addPendingBlock(Block& block) {
    pendingBlocks_.insert_or_assign(block.GetHash(), std::make_shared<Block>(block));
}

void Chain::removePendingBlock(const uint256& hash) {
    pendingBlocks_.erase(hash);
}

bool Chain::isBlockPending(const uint256& hash) const {
    return pendingBlocks_.find(hash) != pendingBlocks_.end();
}

std::vector<std::shared_ptr<const Block>> Chain::GetSortedSubgraph(const std::shared_ptr<const Block> pblock) {
    std::vector<std::shared_ptr<const Block>> stack = {pblock};
    std::vector<std::shared_ptr<const Block>> result;
    std::shared_ptr<const Block> cursor;

    while (!stack.empty()) {
        cursor = stack.back();

        auto swap = pendingBlocks_.find(cursor.get()->getMilestoneHash());
        if (swap != pendingBlocks_.end()) {
            stack.push_back(swap->second);
        }

        swap = pendingBlocks_.find(cursor.get()->getPrevHash());
        if (swap != pendingBlocks_.end()) {
            stack.push_back(swap->second);
        }

        swap = pendingBlocks_.find(cursor.get()->getTIPHash());
        if (swap != pendingBlocks_.end()) {
            stack.push_back(swap->second);
        }

        uint256 cursorHash = cursor.get()->GetHash();
        pendingBlocks_.erase(cursorHash);
        result.push_back(cursor);
        stack.pop_back();
    }

    return result;
}
