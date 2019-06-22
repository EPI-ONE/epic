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
typedef std::unique_ptr<Block> BlockCache;

class Caterpillar {
public:
    Caterpillar() = delete;
    Caterpillar(const std::string& dbPath);

    /* API for other modules for searching a block */
    StoredRecord GetMilestoneAt(size_t height) const;
    StoredRecord GetRecord(const uint256&) const;
    ConstBlockPtr GetBlockCache(const uint256&) const;
    std::vector<StoredRecord> GetLevelSetAt(size_t height) const;
    // TODO: get raw data stream of a level set from file
    VStream GetRawLevelSetAt(size_t height) const;
    size_t GetHeight(const uint256&) const;

    bool DAGExists(const uint256&) const;

    // TODO: search for UTXO in db
    std::unique_ptr<UTXO> GetTransactionOutput(const uint256&);

    bool StoreRecord(const RecordPtr&) const;

    /* Flush records to db. Called by DAGManager only. */
    void Store(const std::vector<NodeRecord>&);

    bool Exists(const uint256&) const;

    bool DBExists(const uint256&) const;

    bool IsMilestone(const uint256&) const;

    /*
     * Submits tasks to a single thread in which it checks its syntax.
     * If the block passes the checking, add them to pendings in dag_manager.
     * Returns true only if the new block is successfully submitted to pendings.
     */
    bool AddNewBlock(const ConstBlockPtr& block, std::shared_ptr<Peer> peer);

    void EnableOBC();

    void DisableOBC();

    /**
     * Blocks the main thread from going forward
     * until CAT completes all the tasks
     */
    void Stop();

    ~Caterpillar();

private:
    ThreadPool verifyThread_;
    ThreadPool obcThread_;

    RocksDBStore dbStore_;
    OrphanBlocksContainer obc_;

    std::atomic<bool> obcEnabled_;

    ConcurrentHashMap<uint256, ConstBlockPtr> blockCache_;

    /**
     * params for file storage
     */
    static const uint32_t fileCapacity_     = 2 ^ 28;
    static const uint16_t epochCapacity_    = UINT_LEAST16_MAX;
    std::atomic<uint32_t> currentBlkEpoch_ = 0;
    std::atomic<uint16_t> currentBlkName_  = 0;
    std::atomic<uint32_t> currentBlkSize_  = 0;
    std::atomic<uint32_t> currentRecEpoch_ = 0;
    std::atomic<uint16_t> currentRecName_  = 0;
    std::atomic<uint32_t> currentRecSize_  = 0;

    bool IsSolid(const ConstBlockPtr&) const;
    bool IsWeaklySolid(const ConstBlockPtr&) const;
    bool AnyLinkIsOrphan(const ConstBlockPtr&) const;
    void Cache(const ConstBlockPtr&);
    bool CheckPuntuality(const ConstBlockPtr& blk, const RecordPtr& ms) const;
    void AddBlockToOBC(const ConstBlockPtr&, const uint8_t& mask);
    void ReleaseBlocks(const uint256&);
};

extern std::unique_ptr<Caterpillar> CAT;

#endif // __SRC_CATERPILLAR_H__
