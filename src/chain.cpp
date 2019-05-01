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
