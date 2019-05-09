#include "obc.h"

OrphanBlocksContainer::~OrphanBlocksContainer() {
    block_dep_map_.clear();
    lose_ends_.clear();
}

std::size_t OrphanBlocksContainer::Size() const {
    return lose_ends_.size();
}

bool OrphanBlocksContainer::Empty() const {
    return this->Size() == 0;
}

bool OrphanBlocksContainer::Contains(const uint256& hash) const {
    return block_dep_map_.find(hash) != block_dep_map_.end();
}

void OrphanBlocksContainer::AddBlock(const BlockPtr& block, bool m_missing, bool t_missing, bool p_missing) {
    /* return if the block is actually not an orphan */
    if (!(m_missing || t_missing || p_missing)) {
        return;
    }

    /* construct new dependency
     * for the new block */
    obc_dep_ptr dep(new obc_dep);
    dep->block = block;
    dep->ndeps = 0;

    /* insert new dependency into block_dep_map_ */
    block_dep_map_.insert_or_assign(block->GetHash(), dep);

    /* used in the following three if clauses */
    uint256 hash;

    if (m_missing) {
        hash    = block->GetMilestoneHash();
        auto it = block_dep_map_.find(hash);

        if (it == block_dep_map_.end()) {
            /* if the milestone is not in this OBC
             * then the dep is a lose end */
            lose_ends_.insert({hash, dep});
        } else {
            /* if the milestone is in the OBC
             * the dep is linked to the milestone dep */
            it->second->deps.push_back(dep);
        }

        dep->ndeps++;
    }

    if (t_missing) {
        hash    = block->GetTipHash();
        auto it = block_dep_map_.find(hash);

        if (it == block_dep_map_.end()) {
            /* if the tip block is not in this OBC
             * then the dep is a lose end */
            lose_ends_.insert({hash, dep});
        } else {
            /* if the tip block is in the OBC
             * the dep is linked to the tip block dep */
            it->second->deps.push_back(dep);
        }

        dep->ndeps++;
    }

    if (p_missing) {
        hash    = block->GetPrevHash();
        auto it = block_dep_map_.find(hash);

        if (it == block_dep_map_.end()) {
            /* if the prev block is not in this OBC
             * then the dep is a lose end */
            lose_ends_.insert({hash, dep});
        } else {
            /* if the prev block is in the OBC
             * the dep is linked to the prev block dep */
            it->second->deps.push_back(dep);
        }

        dep->ndeps++;
    }
}

std::optional<std::vector<BlockPtr>> OrphanBlocksContainer::SubmitHash(const uint256& hash) {
    auto range = lose_ends_.equal_range(hash);

    /* if no lose ends can be tied using this hash return */
    if (std::distance(range.first, range.second) == 0) {
        return {};
    }

    std::vector<obc_dep_ptr> stack;
    std::vector<BlockPtr> result;

    /* for all deps that have the given hash as a parent/dependency */
    for (auto it = range.first; it != range.second; it++) {
        /* push it onto the stack as it might
         * be used later on */
        stack.push_back(it->second);

        /* if the addition of the given hash
         * would tie this lose end remove the
         * dependency from the lose ends map*/
        if (it->second->ndeps == 1) {
            lose_ends_.erase(it);
        }
    }

    obc_dep_ptr cursor;
    while (!stack.empty()) {
        /* pop dependency from
         * the stack*/
        cursor = stack.back();
        stack.pop_back();

        /* decrement the number of
         * missing dependencies */
        cursor->ndeps--;
        if (cursor->ndeps > 0) {
            continue;
        }

        /* by now we know that all blocks are available
         * meaning that the block is return as it is not
         * an orphan anymore */
        result.push_back(cursor->block);

        /* this also means that we have to remove the
         * dependency from the block_dep_map_ */
        block_dep_map_.erase(cursor->block->GetHash());

        /* further we push all dependencies that dependend
         * on this block onto the stack */
        for (const obc_dep_ptr& dep : cursor->deps) {
            stack.push_back(dep);
        }
    }

    return result;
}
