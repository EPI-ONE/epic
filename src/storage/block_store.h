// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef EPIC_STORAGE_H
#define EPIC_STORAGE_H

#include "dag_manager.h"
#include "db.h"
#include "obc.h"
#include "scheduler.h"
#include "threadpool.h"

#include <atomic>
#include <memory>
#include <numeric>
#include <vector>

typedef std::unique_ptr<Vertex, std::function<void(Vertex*)>> StoredVertex;

class BlockStore {
public:
    BlockStore() = delete;
    explicit BlockStore(const std::string& dbPath);

    /**
     * DB API for other modules
     */
    VertexPtr GetMilestoneAt(size_t height) const;
    VertexPtr GetVertex(const uint256&, bool withBlock = true) const;
    ConstBlockPtr GetBlockCache(const uint256&) const;
    ConstBlockPtr FindBlock(const uint256&) const;
    VStream GetRawLevelSetAt(size_t height, file::FileType = file::FileType::BLK) const;
    VStream GetRawLevelSetBetween(size_t height1, size_t height2, file::FileType = file::FileType::BLK) const;
    std::vector<ConstBlockPtr> GetLevelSetBlksAt(size_t height) const;
    std::vector<VertexPtr> GetLevelSetVtcsAt(size_t height, bool withBlock = true) const;
    size_t GetHeight(const uint256&) const;
    uint64_t GetHeadHeight() const;
    bool SaveHeadHeight(uint64_t height) const;
    uint256 GetBestChainWork() const;
    bool SaveBestChainWork(const uint256&) const;
    uint256 GetMinerChainHead() const;
    bool SaveMinerChainHead(const uint256&) const;
    bool ExistsUTXO(const uint256&) const;
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
    bool StoreLevelSet(const std::vector<VertexWPtr>& lvs);
    bool StoreLevelSet(const std::vector<VertexPtr>& lvs);

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
    void AddBlockToOBC(ConstBlockPtr&&, const uint8_t& mask);
    void ReleaseBlocks(const uint256&);
    void EnableOBC();
    void DisableOBC();
    const OrphanBlocksContainer& GetOBC() const;

    void SetFileCapacities(uint32_t, uint16_t);

    /**
     * Blocks the main thread from going forward
     * until STORE completes all the tasks
     */
    void Wait();
    void Stop();

    ~BlockStore();

private:
    ThreadPool obcThread_;
    std::atomic<bool> obcEnabled_;
    OrphanBlocksContainer obc_;
    Scheduler obcTimeout_;

    std::atomic_bool interrupt{false};
    std::thread scheduler_;
    void ScheduleTask();

    DBStore dbStore_;
    ConcurrentHashMap<uint256, ConstBlockPtr> blockPool_;

    /**
     * params for file storage
     */
    uint32_t fileCapacity_                     = 1 << 28;
    uint16_t epochCapacity_                    = UINT_LEAST16_MAX;
    std::atomic_uint_fast32_t currentBlkEpoch_ = 0;
    std::atomic_uint_fast32_t currentVtxEpoch_ = 0;
    std::atomic_uint_fast16_t currentBlkName_  = 0;
    std::atomic_uint_fast16_t currentVtxName_  = 0;
    std::atomic_uint_fast32_t currentBlkSize_  = 0;
    std::atomic_uint_fast32_t currentVtxSize_  = 0;

    uint32_t loadCurrentBlkEpoch();
    uint32_t loadCurrentVtxEpoch();
    uint16_t loadCurrentBlkName();
    uint16_t loadCurrentVtxName();
    uint32_t loadCurrentBlkSize();
    uint32_t loadCurrentVtxSize();

    void CarryOverFileName(std::pair<uint32_t, uint32_t>);
    void AddCurrentSize(std::pair<uint32_t, uint32_t>);

    StoredVertex ConstructNRFromFile(std::optional<std::pair<FilePos, FilePos>>&&, bool withBlock = true) const;
    FilePos& NextFile(FilePos&) const;
};

extern std::unique_ptr<BlockStore> STORE;

#endif // EPIC_STORAGE_H
