// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef EPIC_DAG_MANAGER_H
#define EPIC_DAG_MANAGER_H

#include "chains.h"
#include "sync_messages.h"
#include "threadpool.h"

class Peer;
using PeerPtr = std::shared_ptr<Peer>;
class Vertex;
class UTXO;

struct StatData {
    size_t nTxCnt;
    size_t nBlkCnt;
    uint32_t tStart;

    StatData() : nTxCnt(0), nBlkCnt(0), tStart(0) {}
};

class DAGManager {
public:
    DAGManager();
    ~DAGManager() = default;

    /**
     * Delete the genesis milestone in the chains if last head height >0, maybe reload some blocks as cache later
     */
    bool Init();

    /////////////////////////////// Synchronization /////////////////////////////////////

    /**
     * Called by Cat when a coming block is not solid. Do nothing if isBatchSynching
     * and syncingPeer is not null. This adds the GetBlocksMessage to Peer’s message
     * sending queue according to the BlockLocator constructed by peer.
     */
    void RequestInv(uint256 fromHash, const size_t& len, PeerPtr peer);
    void CallbackRequestInv(std::unique_ptr<Inv> inv, PeerPtr peer);

    /** Called by Peer and sets a Bundle as the callback to the task. */
    void RespondRequestInv(std::vector<uint256>&, uint32_t, PeerPtr);

    /** Called by Peer and sets a Bundle as the callback to the task. */
    void RespondRequestLVS(const std::vector<uint256>&, const std::vector<uint32_t>&, PeerPtr);
    void RespondRequestPending(uint32_t, const PeerPtr&) const;

    /////////////////////////////// Verification /////////////////////////////////////

    /**
     * Submits tasks to a single thread in which it checks its syntax.
     * If the block passes the checking, add them to pendings in dag_manager.
     * Returns true only if the new block is successfully submitted to pendings.
     */
    void AddNewBlock(ConstBlockPtr block, PeerPtr peer);

    /**
     * Checks whether the block links to an old milestone
     */
    bool CheckPuntuality(const ConstBlockPtr& blk, const VertexPtr& ms) const;

    //////////////////////// APIs for retriving data from DAG ////////////////////////

    // The following two methods searches on main chain ONLY.
    VertexPtr GetMainChainVertex(const uint256&) const;
    std::vector<ConstBlockPtr> GetMainChainLevelSet(const uint256&) const;

    /**
     * Returns the level set of the milestone with the given hash.
     * Also searches on forked branches.
     */
    std::vector<VertexPtr> GetLevelSet(const uint256&, bool withBlock = true) const;

    /**
     * Starting from the given hash, traverses the main milestone chain
     * backward/forward by the given length
     */
    std::vector<uint256> TraverseMilestoneBackward(VertexPtr, size_t) const;
    std::vector<uint256> TraverseMilestoneForward(const VertexPtr, size_t) const;

    // Checkout a milestone either in different chain or in db
    VertexPtr GetMsVertex(const uint256&, bool withBlock = true) const;

    ChainPtr GetBestChain() const;
    size_t GetBestMilestoneHeight() const;
    VertexPtr GetMilestoneHead() const;

    const Chains& GetChains() const {
        return milestoneChains_;
    }

    /////////////////////////////// Mics. /////////////////////////////////////

    using OnLvsConfirmedCallback = std::function<void(std::vector<std::shared_ptr<Vertex>>,
                                                      std::unordered_map<uint256, std::shared_ptr<const UTXO>>,
                                                      std::unordered_set<uint256>)>;

    using OnChainUpdatedCallback = std::function<void(ConstBlockPtr, bool)>;

    /**
     * Actions to be performed by wallet when a level set is confirmed
     */
    void RegisterOnLvsConfirmedCallback(OnLvsConfirmedCallback&& callback_func);

    void RegisterOnChainUpdatedCallback(OnChainUpdatedCallback&& func);

    void NotifyOnChainUpdated(ConstBlockPtr block, bool isMainchain) {
        if (onChainUpdatedCallback_) {
            onChainUpdatedCallback_(block, isMainchain);
        }
    }

    bool IsDownloadingEmpty() {
        return downloading_.empty();
    }

    bool EraseDownloading(const uint256& h) {
        return downloading_.erase(h);
    }

    StatData GetStatData() const;

    /**
     * Blocks the main thread from going forward
     * until DAG completes all the tasks
     */
    void Stop();
    void Wait();

private:
    const uint8_t maxGetDataSize      = 5;
    const time_t obcEnableThreshold   = 300;
    const uint32_t sync_task_timeout  = 180; // in seconds
    const uint32_t max_get_inv_length = 1000;

    ThreadPool verifyThread_;
    ThreadPool syncPool_;
    ThreadPool storagePool_;

    /**
     * A list of hashes we've sent out in GetData requests.
     * Should be thread-safe.
     */
    ConcurrentHashSet<uint256> downloading_;

    /**
     * A list of milestone chains, with first element being
     * the main chain and others being forked chains.
     */
    Chains milestoneChains_;

    /**
     * Stores VertexPtr of all verified milestones on all branches as a cache
     */
    ConcurrentHashMap<uint256, VertexPtr> msVertices_;

    /**
     * Listener that triggers when a levelset is confirmed
     */
    OnLvsConfirmedCallback onLvsConfirmedCallback_ = nullptr;

    OnChainUpdatedCallback onChainUpdatedCallback_ = nullptr;

    /**
     * a simple data structure to store statistic data from when the node starts
     */
    StatData stat_;
    mutable std::shared_mutex statLock_;

    void UpdateStatOnLvsStored(const MilestonePtr& pms);

    std::vector<uint256> ConstructLocator(const uint256& fromHash, size_t length, const PeerPtr&);

    /**
     * Start a new thread and create a list of GetData tasks that is either added
     * to preDownloading (if it's not empty) or a peer's task queue. If preDownloading
     * is not empty, drain certain amount of tasks from preDownloading to peer's task queue.
     * Whenever a task is sent to peer, add the hash of the task in the downloading list.
     */
    void RequestData(std::vector<uint256>& requests, const PeerPtr& requestFrom);

    /** Delete the chain who loses in the race competition */
    void DeleteFork();

    /**
     * Adds a newly received block to the corresponding chain
     * that passes syntax checking .
     */
    void AddBlockToPending(const ConstBlockPtr& block);

    void ProcessMilestone(const ChainPtr&, const ConstBlockPtr&);

    bool IsMainChainMS(const uint256&) const;

    size_t GetHeight(const uint256&) const;

    std::vector<ConstBlockPtr> GetMainChainLevelSet(size_t height) const;

    VStream GetMainChainRawLevelSet(size_t height) const;
    VStream GetMainChainRawLevelSet(const uint256&) const;

    bool ExistsNode(const uint256&) const;

    /**
     * Methods that flushing oldest blocks with corresponding node vertices and in memory to hybrid db system
     */

    /**
     * Returns the number of milestones that can be flushed into db
     * zero if no milestones need to be flushed
     */
    void FlushTrigger();

    // flush the oldest milestone
    void FlushToSTORE(MilestonePtr);

    void EnableOBC();
};

bool CheckMsPOW(const ConstBlockPtr& b, const MilestonePtr& m);

extern std::unique_ptr<DAGManager> DAG;

#endif // EPIC_DAG_MANAGER_H
