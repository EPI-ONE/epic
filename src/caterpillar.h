#ifndef __SRC_CATERPILLAR_H__
#define __SRC_CATERPILLAR_H__

#include <atomic>
#include <memory>
#include <numeric>
#include <vector>

#include "dag_manager.h"
#include "obc.h"
#include "rocksdb.h"
#include "threadpool.h"

typedef std::unique_ptr<NodeRecord, std::function<void(NodeRecord*)>> StoredRecord;

class Caterpillar {
public:
    Caterpillar() = delete;
    explicit Caterpillar(const std::string& dbPath);

    /**
     * DB API for other modules
     */
    RecordPtr GetMilestoneAt(size_t height) const;
    StoredRecord GetRecord(const uint256&) const;
    ConstBlockPtr GetBlockCache(const uint256&) const;
    std::vector<ConstBlockPtr> GetLevelSetAt(size_t height) const;
    VStream GetRawLevelSetAt(size_t height) const;
    VStream GetRawLevelSetBetween(size_t height1, size_t heigh2) const;
    size_t GetHeight(const uint256&) const;
    uint64_t GetHeadHeight() const;
    bool SaveHeadHeight(uint64_t height) const;
    uint256 GetMinerChainHead() const;
    bool SaveMinerChainHead(const uint256&) const;

    std::unique_ptr<UTXO> GetUTXO(const uint256&) const;
    bool AddUTXO(const uint256&, const UTXOPtr&) const;
    bool RemoveUTXO(const uint256&) const;

    uint256 GetPrevRedemHash(const uint256&) const;
    bool UpdatePrevRedemHashes(const RegChange&) const;
    bool RollBackPrevRedemHashes(const RegChange&) const;

    /**
     * Flushes a level set to db.
     * Note that this method assumes that the milestone is
     * the first block in the lvs.
     */
    bool StoreLevelSet(const std::vector<RecordWPtr>& lvs);
    bool StoreLevelSet(const std::vector<RecordPtr>& lvs);

    /**
     * Removes block cache when flushing
     */
    void UnCache(const uint256&);

    /**
     * Returns true if the hash exists in DB
     */
    bool DBExists(const uint256&) const;

    /**
     * Returns true if the hash exists in cache or DB
     */
    bool DAGExists(const uint256&) const;

    /**
     * Returns true if the hash exists in one of OBC or DAG
     */
    bool Exists(const uint256&) const;

    /**
     * Returns true if the hash is of a milestone in DB
     * (i.e., confirmed main chain)
     */
    bool IsMilestone(const uint256&) const;

    /**
     * obc and solidity check
     */
    bool IsSolid(const ConstBlockPtr&) const;         // if ancestors are all in DAG (cache + DB)
    bool IsWeaklySolid(const ConstBlockPtr&) const;   // if ancestors are all in either DAG or OBC
    bool AnyLinkIsOrphan(const ConstBlockPtr&) const; // if any ancestor is in OBC
    void Cache(const ConstBlockPtr&);
    void AddBlockToOBC(const ConstBlockPtr&, const uint8_t& mask);
    void ReleaseBlocks(const uint256&);
    void EnableOBC();
    void DisableOBC();

    void SetFileCapacities(uint32_t, uint16_t);

    /**
     * Blocks the main thread from going forward
     * until CAT completes all the tasks
     */
    void Wait();
    void Stop();

    ~Caterpillar();

private:
    ThreadPool obcThread_;

    RocksDBStore dbStore_;
    OrphanBlocksContainer obc_;

    std::atomic<bool> obcEnabled_;

    ConcurrentHashMap<uint256, ConstBlockPtr> blockCache_;

    /**
     * params for file storage
     */
    uint32_t fileCapacity_                     = 1 << 28;
    uint16_t epochCapacity_                    = UINT_LEAST16_MAX;
    std::atomic_uint_fast32_t currentBlkEpoch_ = 0;
    std::atomic_uint_fast32_t currentRecEpoch_ = 0;
    std::atomic_uint_fast16_t currentBlkName_  = 0;
    std::atomic_uint_fast16_t currentRecName_  = 0;
    std::atomic_uint_fast32_t currentBlkSize_  = 0;
    std::atomic_uint_fast32_t currentRecSize_  = 0;

    uint32_t loadCurrentBlkEpoch();
    uint32_t loadCurrentRecEpoch();
    uint16_t loadCurrentBlkName();
    uint16_t loadCurrentRecName();
    uint32_t loadCurrentBlkSize();
    uint32_t loadCurrentRecSize();

    void CarryOverFileName(std::pair<uint32_t, uint32_t>);
    void AddCurrentSize(std::pair<uint32_t, uint32_t>);

    StoredRecord ConstructNRFromFile(std::optional<std::pair<FilePos, FilePos>>&&) const;
    FilePos& NextFile(FilePos&) const;
};

extern std::unique_ptr<Caterpillar> CAT;

#endif // __SRC_CATERPILLAR_H__
