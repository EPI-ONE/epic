// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "obc.h"
#include <memory>
#include <queue>

#define READER_LOCK(mu) std::shared_lock<std::shared_mutex> reader(mu);
#define WRITER_LOCK(mu) std::unique_lock<std::shared_mutex> writer(mu);

std::size_t OrphanBlocksContainer::Size() const {
    READER_LOCK(mutex_)
    return obcBlockSize_;
}

bool OrphanBlocksContainer::Empty() const {
    READER_LOCK(mutex_)
    return obcBlockSize_ == 0;
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
    obcBlockSize_++;

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
    WRITER_LOCK(mutex_)
    auto entry = block_dep_map_.find(hash);
    if (entry == block_dep_map_.end()) {
        return {};
    }
    std::vector<ConstBlockPtr> result;
    // release only one level of blocks(its children blocks)
    for (auto& n : entry->second->deps) {
        n->ndeps--;

        if (n->ndeps > 0) {
            continue;
        }

        result.push_back(n->block);
        n->block = nullptr;
        obcBlockSize_--;
    }

    block_dep_map_.erase(entry);
    return result;
}

size_t OrphanBlocksContainer::Prune(uint32_t secs) {
    std::vector<uint256> pruned_block;
    std::unordered_set<uint256> old_dependency;

    // to find the obc_dependency which doesn't have a block instance
    auto check_old_dependency = [&](const uint256& parent_hash, std::unordered_set<uint256>& hashes) {
        auto it = block_dep_map_.find(parent_hash);
        if (it != block_dep_map_.end() && !it->second->block) {
            hashes.insert(parent_hash);
        }
    };

    {
        READER_LOCK(mutex_)
        uint64_t current_time = time(nullptr);
        for (auto& it : block_dep_map_) {
            if (it.second->block && it.second->block->GetTime() + secs <= current_time) {
                pruned_block.push_back(it.first);
                check_old_dependency(it.second->block->GetMilestoneHash(), old_dependency);
                check_old_dependency(it.second->block->GetPrevHash(), old_dependency);
                check_old_dependency(it.second->block->GetTipHash(), old_dependency);
            }
        }
    }

    for (const auto& blk_hash : old_dependency) {
        DeleteBlockTree(blk_hash);
    }

    spdlog::debug("Pruned {} blocks and {} void blocks in obc", pruned_block.size(), old_dependency.size());

    return pruned_block.size() + old_dependency.size();
}

size_t OrphanBlocksContainer::GetDepNodeSize() const {
    READER_LOCK(mutex_)
    return block_dep_map_.size();
}

void OrphanBlocksContainer::DeleteBlockTree(const uint256& hash) {
    WRITER_LOCK(mutex_)
    auto entry = block_dep_map_.find(hash);
    if (entry == block_dep_map_.end()) {
        return;
    }

    std::queue<obc_dep_ptr> toBeDeleted;
    for (auto& n : entry->second->deps) {
        toBeDeleted.push(n);
    }


    if (entry->second->block) {
        obcBlockSize_--;
    }
    block_dep_map_.erase(entry);

    // delete the tree starting from the root using BFS
    obc_dep_ptr cursor;
    while (!toBeDeleted.empty()) {
        cursor = toBeDeleted.front();
        toBeDeleted.pop();

        if (block_dep_map_.erase(cursor->block->GetHash())) {
            obcBlockSize_--;
        }

        for (const obc_dep_ptr& dep : cursor->deps) {
            toBeDeleted.push(dep);
        }
    }
}
