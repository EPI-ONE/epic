// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "obc.h"
#include <memory>

#define READER_LOCK(mu) std::shared_lock<std::shared_mutex> reader(mu);
#define WRITER_LOCK(mu) std::unique_lock<std::shared_mutex> writer(mu);

std::size_t OrphanBlocksContainer::Size() const {
    READER_LOCK(mutex_)
    return size;
}

bool OrphanBlocksContainer::Empty() const {
    READER_LOCK(mutex_)
    return block_dep_map_.empty();
}

bool OrphanBlocksContainer::Contains(const uint256& hash) const {
    READER_LOCK(mutex_)
    auto entry = block_dep_map_.find(hash);
    return entry != block_dep_map_.end() && entry->second->block;
}

void OrphanBlocksContainer::AddBlock(ConstBlockPtr&& block, uint8_t missing_mask) {
    if (missing_mask == 0) {
        return;
    }

    auto bHash    = block->GetHash();
    auto msHash   = block->GetMilestoneHash();
    auto prevHash = block->GetPrevHash();
    auto tipHash  = block->GetTipHash();

    // construct new dependency for the new block
    obc_dep_ptr dep;

    READER_LOCK(mutex_)
    auto entry = block_dep_map_.find(bHash);
    if (entry != block_dep_map_.end()) {
        dep = entry->second;
    } else {
        dep = std::make_shared<obc_dep>();
    }
    reader.unlock();

    dep->block = std::move(block);

    // insert new dependency into block_dep_map_
    std::unordered_set<uint256> unique_missing_hashes;

    auto common_insert = [&](const uint256& parent_hash) {
        auto it = block_dep_map_.find(parent_hash);
        unique_missing_hashes.insert(parent_hash);

        if (it == block_dep_map_.end()) {
            // if the dependency is not in this OBC then the dep is a lose end
            auto parent_dep = std::make_shared<obc_dep>();
            parent_dep->deps.emplace(dep);
            block_dep_map_.emplace(parent_hash, std::move(parent_dep));
        } else {
            // if the dependency is in the OBC the dep is linked to the dependency dep
            it->second->deps.emplace(dep);
        }
    };

    WRITER_LOCK(mutex_)
    block_dep_map_.insert_or_assign(bHash, dep);
    size++;

    if (missing_mask & M_MISSING) {
        common_insert(msHash);
    }

    if (missing_mask & T_MISSING) {
        common_insert(tipHash);
    }

    if (missing_mask & P_MISSING) {
        common_insert(prevHash);
    }

    dep->ndeps = unique_missing_hashes.size();
}

std::vector<ConstBlockPtr> OrphanBlocksContainer::SubmitHash(const uint256& hash) {
    WRITER_LOCK(mutex_);
    auto entry = block_dep_map_.find(hash);
    if (entry == block_dep_map_.end()) {
        return {};
    }

    std::vector<obc_dep_ptr> stack;
    std::vector<ConstBlockPtr> result;

    // Push all dependencies of the entry to stack to recursively
    // release dependencies of the dependencies
    for (auto& n : entry->second->deps) {
        stack.push_back(n);
    }

    if (entry->second->block) {
        size--;
    }
    block_dep_map_.erase(entry);
    writer.unlock();

    obc_dep_ptr cursor;
    while (!stack.empty()) {
        // pop dependency from the stack
        cursor = stack.back();
        stack.pop_back();

        // decrement the number of missing dependencies
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
        writer.lock();
        if (block_dep_map_.erase(cursor->block->GetHash())) {
            size--;
        }
        writer.unlock();

        // further we push all dependencies of this block onto the stack
        for (const obc_dep_ptr& dep : cursor->deps) {
            stack.push_back(dep);
        }
    }

    return result;
}

size_t OrphanBlocksContainer::Prune(uint32_t secs) {
    return 0;
}
