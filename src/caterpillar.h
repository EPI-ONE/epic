#ifndef __SRC_CATERPILLAR_H__
#define __SRC_CATERPILLAR_H__

#include <list>
#include <vector>

#include "block.h"
#include "dag_manager.h"
#include "obc.h"
#include "rocksdb.h"
#include "threadpool.h"

typedef std::unique_ptr<NodeRecord> DBRecord;

class Caterpillar {
public:
    Caterpillar() = delete;
    Caterpillar(std::string& dbPath);

    /* API for other modules for searching a block */
    DBRecord GetBlock(const uint256&) const;

    bool StoreRecord(const RecordPtr&) const;

    /* Flush records to db. Called by DAGManager only. */
    void Store(const std::vector<NodeRecord>&);

    /*
     * Submits tasks to a single thread in which it checks its syntax.
     * If the block passes the checking, add them to pendings in dag_manager.
     * Returns true only if the new block is successfully submitted to pendings.
     */
    bool AddNewBlock(const ConstBlockPtr& block, const Peer* peer);

    /*
     * Blocks the main thread from going forward
     * before CAT completes all the tasks
     * For test only.
     */
    void Stop() {
        while (obcThread_.GetTaskSize() > 0) {
            std::this_thread::yield();
        }
        obcThread_.Stop();

        while (verifyThread_.GetTaskSize() > 0) {
            std::this_thread::yield();
        }
        verifyThread_.Stop();
    }

    ~Caterpillar();

private:
    ThreadPool verifyThread_;
    ThreadPool obcThread_;
    RocksDBStore dbStore_;
    OrphanBlocksContainer obc_;

    inline bool Exists(const uint256& blockHash) const {
        return dbStore_.Exists(blockHash) || obc_.IsOrphan(blockHash);
    }

    bool CheckPuntuality(const ConstBlockPtr& blk, const DBRecord& ms) const;
    void ReleaseBlocks(const uint256&);
};

#endif // __SRC_CATERPILLAR_H__
