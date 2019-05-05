#ifndef __SRC_CATERPILLAR_H__
#define __SRC_CATERPILLAR_H__

#include <list>
#include <vector>

#include "block.h"
#include "rocksdb.h"
#include "threadpool.h"

class OrphanBlocksContainer {
public:
    OrphanBlocksContainer();

    bool Add(BlockPtr& b);

    void Remove(BlockPtr& b);

    bool Contains(uint256& blockHash);

    bool IsEmpty();

    size_t size();

    void ReleaseBlocks(uint256& blockHash);

    ~OrphanBlocksContainer();

private:
    std::unordered_map<uint256, std::unordered_set<BlockPtr>> ODict;
    std::unordered_map<int, std::unordered_set<uint256>> solidDegree;

    std::vector<BlockPtr> updateOne(uint256& blockHash);
};

class Caterpillar {
public:
    Caterpillar(std::string& dbPath);

    // will return a pointer-like structure later
    Block* GetBlock(const uint256& bhash) const;

    // flush blocks to db + file storage system
    void Store(const std::vector<BlockPtr> vblock);

    /* Submits tasks to a single thread in which it checks its syntax.
     * If all block pass the checking,
     * add them to the corresponding chain in dag_manager.
     */
    bool AddNewBlock(const BlockPtr block);

    ~Caterpillar();

private:
    ThreadPool thread_ = ThreadPool(1);
    RocksDBStore dbStore_;
    OrphanBlocksContainer obc_;

    bool Exisis(uint256& blockHash) {
        return dbStore_.Exists(blockHash) || obc_.Contains(blockHash);
    }
};

#endif // __SRC_CATERPILLAR_H__
