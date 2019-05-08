#include "chain.h"

Chain::Chain(bool mainchain) : ismainchain_(mainchain) {
    pendingBlocks_ = {};
    vmilestones_   = {std::make_shared<ChainState>()};
}

std::shared_ptr<ChainState> Chain::GetChainHead() const {
    return vmilestones_.back();
}

Chain::Chain(const Chain& chain)
    : ismainchain_(false), vmilestones_(chain.vmilestones_), pendingBlocks_(chain.pendingBlocks_) {}

Chain::Chain(const Chain& chain, std::shared_ptr<ChainState> pfork) : ismainchain_(false), pendingBlocks_(chain.pendingBlocks_) {
    std::shared_ptr<ChainState> cursor = chain.GetChainHead();
    std::shared_ptr<ChainState> target = pfork->pprevious;
    while (cursor != target) {
        for (const auto& pblock : cursor->vblockstore) {
            pendingBlocks_.insert({pblock->cBlock->GetHash(), pblock->cBlock});
        }
        cursor = cursor->pprevious;
        assert(cursor);
    }
}

std::shared_ptr<ChainState> Chain::FindSplit(const Chain& pchain) const {
    std::shared_ptr<ChainState> cursor = GetChainHead();
    std::shared_ptr<ChainState> newCursor = pchain.GetChainHead();

    while (cursor != newCursor) {
        if (cursor->height > newCursor->height) {
            cursor = cursor->pprevious;
            assert(cursor);
        } else {
            newCursor = newCursor->pprevious;
            assert(newCursor);
        }
    }

    return newCursor;
}

void Chain::AddPendingBlock(ConstBlockPtr pblock) {
    pendingBlocks_.insert_or_assign(pblock->GetHash(), pblock);
}
    
void Chain::AddPendingBlock(const Block& block) {
    pendingBlocks_.insert_or_assign(block.GetHash(), std::make_shared<const BlockNet>(block));
}

void Chain::RemovePendingBlock(const uint256& hash) {
    pendingBlocks_.erase(hash);
}

bool Chain::IsBlockPending(const uint256& hash) const {
    return pendingBlocks_.find(hash) != pendingBlocks_.end();
}

std::size_t Chain::GetPendingBlockCount() const {
    return pendingBlocks_.size();
}

std::vector<std::shared_ptr<const Block>> Chain::GetSortedSubgraph(const Block& pblock) {
    return GetSortedSubgraph(std::make_shared<const Block>(pblock));
}

std::vector<std::shared_ptr<const Block>> Chain::GetSortedSubgraph(const std::shared_ptr<const Block> pblock) {
    std::vector<std::shared_ptr<const Block>> stack = {pblock};
    std::vector<std::shared_ptr<const Block>> result;
    std::shared_ptr<const Block> cursor;

    /* reserve a good chunk of memory for efficiency;
     * please note that n/2 is a not really precise
     * upper bound and can be improved */
    stack.reserve(pendingBlocks_.size() / 2);
    result.reserve(pendingBlocks_.size());

    while (!stack.empty()) {
        cursor = stack.back();

        auto swap = pendingBlocks_.find(cursor->GetMilestoneHash());
        if (swap != pendingBlocks_.end()) {
            stack.push_back(swap->second);
            continue;
        }

        swap = pendingBlocks_.find(cursor->GetPrevHash());
        if (swap != pendingBlocks_.end()) {
            stack.push_back(swap->second);
            continue;
        }

        swap = pendingBlocks_.find(cursor->GetTipHash());
        if (swap != pendingBlocks_.end()) {
            stack.push_back(swap->second);
            continue;
        }

        uint256 cursorHash = cursor->GetHash();
        pendingBlocks_.erase(cursorHash);
        result.push_back(cursor);
        stack.pop_back();
    }

    result.shrink_to_fit();
    return result;
}
