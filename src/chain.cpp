#include "chain.h"
#include "caterpillar.h"

Chain::Chain(bool mainchain) : ismainchain_(mainchain) {
    pendingBlocks_ = {};
    recordHistory_ = {};
    states_        = {std::make_shared<ChainState>()};
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
    assert(recordHistory_.at(target));

    for (auto it = states_.rbegin(); (*it)->GetMilestoneHash() != target && it != states_.rend(); it++) {
        for (const auto& h : (*it)->GetRecordHashes()) {
            pendingBlocks_.insert({h, recordHistory_[h]->cblock});
            recordHistory_.erase(h);
            // TODO: roll back UTXOs
        }
        states_.erase(std::next(it).base());
    }
    // note that we don't do any verification here
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

bool Chain::IsValidDistance(const RecordPtr b, const arith_uint256 ms_hashrate) {
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

void Chain::Verify(const ConstBlockPtr pblock) {
    // get a path for validation by the post ordered DFS search
    std::vector<ConstBlockPtr> blocksToValidate = GetSortedSubgraph(pblock);

    std::vector<RecordPtr> recs;
    std::vector<uint256> hashes;
    recs.reserve(blocksToValidate.size());
    hashes.reserve(blocksToValidate.size());

    for (const auto& b : blocksToValidate) {
        recs.emplace_back(std::make_shared<NodeRecord>(b));
        hashes.emplace_back(b->GetHash());
    }
    auto state = make_shared_ChainState(GetChainHead(), *recs.back(), std::move(hashes));

    // validate each block in order
    for (auto& rec : recs) {
        if (!rec->cblock->IsFirstRegistration())  {
            if (auto update = Validate(*rec)) {
                // TODO: remove UTXO from chain
                state->UpdateTXOC(std::move(*update));
            } else {
                rec->validity = NodeRecord::INVALID;
                // TODO: we may add log here
            }
        }
        recordHistory_.insert({rec->cblock->GetHash(), rec});
    }
    states_.emplace_back(state);
}

std::optional<TXOC> Chain::Validate(NodeRecord& record) {
    auto pblock = record.cblock;
    std::optional<TXOC> result;
    if (pblock->HasTransaction()) {
        if (pblock->IsRegistration()) {
            result = ValidateRedemption(record);
        } else {
            result = ValidateTx(record);
        }
    }
    record.UpdateReward(recordHistory_[pblock->GetHash()]->cumulativeReward);
    return result;
}

std::optional<TXOC> Chain::ValidateRedemption(NodeRecord& record) {
    Coin valueIn{};
    Coin valueOut{};
    auto tx = record.cblock->GetTransaction();
    
    // check Transaction distance 

    // update TXOC
    uint64 sigOps = 0;
    TXOC txoc{};
    return {};
}

std::optional<TXOC> Chain::ValidateTx(NodeRecord& record) {
    return {};
}

