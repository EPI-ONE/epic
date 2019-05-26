#include "chain.h"
#include "caterpillar.h"

Chain::Chain(bool mainchain) : ismainchain_(mainchain) {
    states_.push_back(make_shared_ChainState());
}

ChainStatePtr Chain::GetChainHead() const {
    return states_.back();
}

Chain::Chain(const Chain& chain) : ismainchain_(false), states_(chain.states_), pendingBlocks_(chain.pendingBlocks_) {}

Chain::Chain(const Chain& chain, ConstBlockPtr pfork)
    : ismainchain_(false), states_(chain.states_), pendingBlocks_(chain.pendingBlocks_),
      recordHistory_(chain.recordHistory_) {
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

void Chain::AddPendingUTXOs(const std::vector<UTXOPtr>& utxos) {
    if (utxos.empty()) {
        return;
    }
    for (const auto& u : utxos) {
        pendingUTXOs_.emplace(u->GetKey(), u);
    }
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

std::vector<ConstBlockPtr> Chain::GetSortedSubgraph(ConstBlockPtr pblock) {
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

bool Chain::IsValidDistance(const RecordPtr& b, const arith_uint256& ms_hashrate) {
    auto current_distance = UintToArith256(b->cblock->GetTxHash()) ^ UintToArith256(b->cblock->GetPrevHash());
    arith_uint256 S       = 0;
    arith_uint256 t       = 0;

    auto curr = CAT->GetRecord(b->cblock->GetPrevHash());
    for (size_t i = 0; i < params.sortitionThreshold && *curr != GENESIS_RECORD;
         ++i, curr = CAT->GetRecord(curr->cblock->GetPrevHash())) {
        S += curr->cblock->GetChainWork();
        t = curr->cblock->GetTime();
    }

    auto allowed_distance = (params.maxTarget / params.sortitionCoefficient) * S / (ms_hashrate * t);
    return current_distance <= allowed_distance;
}

bool Chain::CheckMsPOW(const ConstBlockPtr& b, const ChainStatePtr& m) {
    return !(UintToArith256(b->GetHash()) > m->milestoneTarget);
}

MilestoneStatus Chain::IsMilestone(const ConstBlockPtr& pblock) {
    auto entry = recordHistory_.find(pblock->GetMilestoneHash());
    ChainStatePtr ms;

    if (entry == recordHistory_.end()) {
        auto rec = CAT->GetRecord(pblock->GetMilestoneHash());
        if (!rec) {
            return WE_DONT_KNOW;
        }
        ms = rec->snapshot;
    } else {
        ms = entry->second->snapshot;
    }

    if (*ms == *GetChainHead() && CheckMsPOW(pblock, ms)) {
        return IS_TRUE_MILESTONE;
    }

    if (ms && CheckMsPOW(pblock, ms)) {
        return IS_FAKE_MILESTONE;
    }

    return IS_NOT_MILESTONE;
}

void Chain::Verify(ConstBlockPtr) {}

void Chain::UpdateChainState(const std::vector<RecordPtr>&) {}
