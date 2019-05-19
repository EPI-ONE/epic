#ifndef __SRC_CATERPILLAR_H__
#define __SRC_CATERPILLAR_H__

#include <list>
#include <memory>
#include <vector>

#include "block.h"
#include "dag_manager.h"
#include "obc.h"
#include "rocksdb.h"
#include "threadpool.h"

typedef std::unique_ptr<NodeRecord> StoredRecord;
typedef std::unique_ptr<BlockNet> BlockCache;

class Caterpillar {
public:
    Caterpillar() = delete;
    Caterpillar(const std::string& dbPath);

    /* API for other modules for searching a block */
    StoredRecord GetRecord(const uint256&) const;
    BlockCache GetBlockCache(const uint256&) const;
    bool IsSolid(const uint256&) const;

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
     * until CAT completes all the tasks
     *
     * FOR TEST ONLY!
     */
    void Stop();

    ~Caterpillar();

private:
    ThreadPool verifyThread_;
    ThreadPool obcThread_;
    RocksDBStore dbStore_;
    OrphanBlocksContainer obc_;

    bool Exists(const uint256&) const;
    bool IsWeaklySolid(const ConstBlockPtr&) const;
    bool AnyLinkIsOrphan(const ConstBlockPtr&) const;
    void Cache(const ConstBlockPtr&) const;
    bool CheckPuntuality(const ConstBlockPtr& blk, const StoredRecord& ms) const;
    void ReleaseBlocks(const uint256&);
};

extern std::unique_ptr<Caterpillar> CAT;

#endif // __SRC_CATERPILLAR_H__
