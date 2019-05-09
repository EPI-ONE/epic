#ifndef __SRC_CATERPILLAR_H__
#define __SRC_CATERPILLAR_H__

#include <list>
#include <vector>

#include "block.h"
#include "dag_manager.h"
#include "obc.h"
#include "rocksdb.h"
#include "threadpool.h"

static DAGManager& DAG = DAGManager::GetDAGManager();

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

    inline bool Exists(const uint256& blockHash) const {
        return dbStore_.Exists(blockHash) || obc_.Contains(blockHash);
    }

    bool CheckPuntuality(const BlockPtr& blk, std::unique_ptr<Block> ms = nullptr) const;
};

#endif // __SRC_CATERPILLAR_H__
