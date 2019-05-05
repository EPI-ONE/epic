#ifndef __SRC_CATERPILLAR_H__
#define __SRC_CATERPILLAR_H__

#include <list>
#include <vector>

#include "block.h"
#include "dag_manager.h"
#include "rocksdb.h"
#include "threadpool.h"

static DAGManager& DAG = DAGManager::GetDAGManager();

class OrphanBlocksContainer {
public:
    OrphanBlocksContainer() : thread_(1) {}

    bool Add(const BlockPtr& b);

    void Remove(const BlockPtr& b);

    bool Contains(const uint256& blockHash) const;

    inline bool IsEmpty() {
        return ODict_.empty();
    }

    inline size_t size() {
        size_t size = 0;
        for (auto& it: solidDegree_) {
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

class Caterpillar {
public:
    Caterpillar() = delete;
    Caterpillar(std::string& dbPath);

    // will return a pointer-like structure later
    Block* GetBlock(const uint256& bhash) const;

    // flush blocks to db + file storage system
    void Store(const std::vector<BlockPtr> vblock);

    /* Submits tasks to a single thread in which it checks its syntax.
     * If the block passes the checking, add them to the corresponding chain in dag_manager.
     * Returns true only if the new block successfully updates pending.
     */
    bool AddNewBlock(const BlockPtr& block, const Peer& peer);

    ~Caterpillar();

private:
    ThreadPool thread_;
    RocksDBStore dbStore_;
    OrphanBlocksContainer obc_;

    inline bool Exisis(const uint256& blockHash) const {
        return dbStore_.Exists(blockHash) || obc_.Contains(blockHash);
    }

    bool CheckPuntuality(const BlockPtr& blk, BlockPtr ms = nullptr);
};

#endif // __SRC_CATERPILLAR_H__
