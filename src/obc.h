#ifndef __SRC_OBC_H__
#define __SRC_OBC_H__

#include <vector>

#include "block.h"

class OrphanBlocksContainer {
public:
    OrphanBlocksContainer() : thread_(1) {}

    bool Add(const BlockPtr& b);

    void Remove(const BlockPtr& b);

    bool Contains(const uint256& blockHash) const;

    inline bool IsEmpty() {
        return ODict_.empty();
    }

    size_t size() {
        size_t size = 0;
        for (auto& it : solidDegree_) {
            size += it.second.size();
        }
        return size;
    }

    /*
     * Submits a task to thread_ that releases any block from OBC
     * that becomes solid with blockHash being its antecedent.
     */
    void ReleaseBlocks(const uint256& blockHash);

    ~OrphanBlocksContainer();

private:
    ThreadPool thread_;
    std::unordered_map<uint256, std::unordered_set<BlockPtr>> ODict_;
    std::unordered_map<uint8_t, std::unordered_set<uint256>> solidDegree_;
    std::vector<BlockPtr> updateOne(uint256& blockHash);
};


#endif
