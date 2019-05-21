#include "chain.h"

Chain::Chain(bool mainchain) : ismainchain_(mainchain) {
    pendingBlocks_ = {};
    recordHistory_ = {};
    states_ = {std::make_shared<ChainState>()};
}

ChainStatePtr Chain::GetChainHead() const {
    return states_.back();
}

Chain::Chain(const Chain& chain)
    : ismainchain_(false), states_(chain.states_), pendingBlocks_(chain.pendingBlocks_) {}

Chain::Chain(const Chain& chain, ConstBlockPtr pfork)
    : ismainchain_(false), states_(chain.states_), pendingBlocks_(chain.pendingBlocks_), recordHistory_(chain.recordHistory_){
    ChainStatePtr cursor = chain.GetChainHead();
    uint256 target       = pfork->GetMilestoneHash();

    for (auto it = states_.rbegin(); (*it)->GetMilestoneHash() != target && it != states_.rend(); it++) {
        // TODO: add logic to pendingBlocks_ and recordHistory_ now
        states_.erase(std::next(it).base());
    }
}

void Chain::AddPendingBlock(ConstBlockPtr pblock) {
    pendingBlocks_.insert_or_assign(pblock->GetHash(), pblock);
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

std::vector<ConstBlockPtr> Chain::GetSortedSubgraph(const ConstBlockPtr pblock) {
    std::vector<ConstBlockPtr> stack = {pblock};
    std::vector<ConstBlockPtr> result;
    ConstBlockPtr cursor;

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
