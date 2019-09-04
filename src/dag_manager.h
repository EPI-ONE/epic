#ifndef __SRC_DAG_MANAGER_H__
#define __SRC_DAG_MANAGER_H__

#include <atomic>
#include <functional>
#include <list>

#include "chains.h"
#include "concurrent_container.h"
#include "sync_messages.h"
#include "task.h"
#include "threadpool.h"

class Peer;
typedef std::shared_ptr<Peer> PeerPtr;

class DAGManager {
public:
    DAGManager();
    ~DAGManager();

    /**
     * Delete the genesis state in the chains if last head height >0, maybe reload some blocks as cache later
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

    std::shared_ptr<GetDataTask> RequestData(const uint256& hash);

    /** Called by Peer and sets a Bundle as the callback to the task. */
    void RespondRequestInv(std::vector<uint256>&, uint32_t, PeerPtr);

    /** Called by Peer and sets a Bundle as the callback to the task. */
    void RespondRequestLVS(const std::vector<uint256>&, const std::vector<uint32_t>&, PeerPtr);
    void RespondRequestPending(uint32_t, const PeerPtr&) const;

    /////////////////////////////// Verification /////////////////////////////////////

    /*
     * Submits tasks to a single thread in which it checks its syntax.
     * If the block passes the checking, add them to pendings in dag_manager.
     * Returns true only if the new block is successfully submitted to pendings.
     */
    void AddNewBlock(ConstBlockPtr block, PeerPtr peer);

    //////////////////////// APIs for retriving data from DAG ////////////////////////

    // The following two methods searches on main chain ONLY.
    RecordPtr GetMainChainRecord(const uint256&) const;
    std::vector<ConstBlockPtr> GetMainChainLevelSet(const uint256&) const;

    /**
     * Returns the level set of the milestone with the given hash.
     * Also searches on forked branches.
     */
    std::vector<RecordPtr> GetLevelSet(const uint256&, bool withBlock = true) const;

    /**
     * Starting from the given hash, traverses the main milestone chain
     * backward/forward by the given length
     */
    std::vector<uint256> TraverseMilestoneBackward(size_t cursorHeight, size_t) const;
    std::vector<uint256> TraverseMilestoneForward(const NodeRecord&, size_t) const;

    // Checkout states either in different chain or in db
    RecordPtr GetState(const uint256&, bool withBlock = true) const;

    Chain& GetBestChain() const;
    size_t GetBestMilestoneHeight() const;
    RecordPtr GetMilestoneHead() const;

    const Chains& GetChains() const {
        return milestoneChains;
    }

    /////////////////////////////// Mics. /////////////////////////////////////

    using OnLvsConfirmedCallback =
        std::function<void(std::vector<RecordPtr>, std::unordered_map<uint256, UTXOPtr>, std::unordered_set<uint256>)>;

    /**
     * Actions to be performed by wallet when a level set is confirmed
     */
    void RegisterOnLvsConfirmedCallback(OnLvsConfirmedCallback&& callback_func);
    bool IsDownloadingEmpty() {
        return downloading.empty();
    }

    void EraseDownloading(uint256& h) {
        downloading.erase(h);
    }

    /**
     * Blocks the main thread from going forward
     * until DAG completes all the tasks
     */
    void Stop();
    void Wait();

private:
    const uint8_t maxGetDataSize    = 5;
    const time_t obcEnableThreshold = 300;
    uint32_t sync_task_timeout      = 30; // in seconds

    ThreadPool verifyThread_;
    ThreadPool syncPool_;
    ThreadPool storagePool_;

    /** Indicator of whether we are storing level sets to CAT. */
    std::atomic<bool> isStoring;

    /** Indicator of whether the DAG manager is doing off-line verification */
    std::atomic<bool> isVerifying;

    /**
     * A list of hashes we've sent out in GetData requests.
     * Should be thread-safe.
     */
    ConcurrentHashSet<uint256> downloading;

    /**
     * A list of milestone chains, with first element being
     * the main chain and others being forked chains.
     */
    Chains milestoneChains;

    /**
     * Stores RecordPtr of all verified milestones on all branches as a cache
     */
    ConcurrentHashMap<uint256, RecordPtr> globalStates_;

    /**
     * Listener that triggers when a levelset is confirmed
     */
    OnLvsConfirmedCallback onLvsConfirmedCallback = nullptr;

    std::vector<uint256> ConstructLocator(const uint256& fromHash, size_t length, const PeerPtr&);

    /**
     * Start a new thread and create a list of GetData tasks that is either added
     * to preDownloading (if it's not empty) or a peer's task queue. If preDownloading
     * is not empty, drain certain amount of tasks from preDownloading to peer's task queue.
     * Whenever a task is sent to peer, add the hash of the task in the downloading list.
     */
    void RequestData(std::vector<uint256>& requests, const PeerPtr& requestFrom);

    /**
     * Removes a verified ms hash from the downloading queue, and start another
     * round of batch sync when the downloading queue is empty.
     * Returns whether the hash is removed successfully.
     */
    bool UpdateDownloadingQueue(const uint256&);

    /** Delete the chain who loses in the race competition */
    void DeleteFork();

    bool CheckPuntuality(const ConstBlockPtr& blk, const RecordPtr& ms) const;

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
     * Methods that flushing oldest blocks with corresponding node records and in memory to hybrid db system
     */

    /**
     * Returns the number of chain states that can be flushed into db
     * zero if no chain states need to be flushed
     */
    size_t FlushTrigger();

    void FlushToCAT(size_t); // flush the oldest chain states
};

bool CheckMsPOW(const ConstBlockPtr& b, const ChainStatePtr& m);

extern std::unique_ptr<DAGManager> DAG;

#endif // __SRC_DAG_MANAGER_H__
