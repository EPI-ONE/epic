#ifndef __SRC_DAG_MANAGER_H__
#define __SRC_DAG_MANAGER_H__

#include <atomic>
#include <list>
#include <queue>

#include "chain.h"
#include "max_queue.h"
#include "task.h"

class Peer;
class DAGManager {
public:
    static DAGManager& GetDAGManager() {
        static DAGManager DAG;
        return DAG;
    }

    /**
     * TODO: For test only. Remove this in the future
     *       when AddBlockToPending becomes real
     */
    std::vector<ConstBlockPtr> pending;

    /**
     * A list of hashes we've sent out in GetData requests.
     * It can be operated by multi threads.
     */
    std::vector<uint256> downloading;

    /**
     * A list of tasks we've prepared to deliver to a Peer.
     * Should be thread-safe.
     */
    std::vector<GetDataTask> preDownloading;

    /** Indicator of whether we are synching with some peer. */
    std::atomic<bool> isBatchSynching;

    /** The peer we are synching with. Null if isBatchSynching is false. */
    std::shared_ptr<Peer> syncingPeer;

    DAGManager();

    /**
     * Called by Cat when a coming block is not solid. Do nothing if isBatchSynching
     * and syncingPeer is not null. This adds the GetBlocksMessage to Peer’s message
     * sending queue according to the BlockLocator constructed by peer.
     */
    void RequestInv(const uint256& fromHash, const size_t& len, std::shared_ptr<Peer> peer);

    /** Called by Peer and sets a list of inventory as the callback to the task. */
    void AssembleInv(GetBlockTask& task);

    /** Called by batchSync to create a GetDataTask for a given hash. */
    GetDataTask RequestData(const uint256& hash, GetDataTask::Type& type, std::shared_ptr<Peer> peer);

    /** Called by Peer and sets a Bundle as the callback to the task. */
    void GetBundle(GetDataTask& task);

    /**
     * Adds a newly received block to the corresponding chain
     * that passes syntax checking .
     */
    void AddBlockToPending(const ConstBlockPtr& block);

    const RecordPtr GetHead() const;

    size_t GetBestMilestoneHeight();

    // Checkout states either in different chain or in db
    RecordPtr GetState(const uint256&);

    const Chain& GetBestChain() const;

    /**
     * Methods are called when the synchronization status is changed:
     * on to off and off to on. Modifies the atomic_flag isBatchSynching.
     */
    void StartBatchSync(std::shared_ptr<Peer> peer);
    void CompleteBatchSync();

private:
    std::unordered_map<uint256, RecordPtr> globalStates_;

    std::atomic<bool> isVerifying;

    /**
     * A list of milestone chains, with first element being
     * the main chain and others being forked chains.
     */
    Chains milestoneChains;

    /**
     * Start a new thread and create a list of GetData tasks that is either added
     * to preDownloading (if it's not empty) or a peer's task queue. If preDownloading
     * is not empty, drain certain amount of tasks from preDownloading to peer's task queue.
     * Whenever a task is sent to peer, add the hash of the task in the downloading list.
     */
    void BatchSync(std::list<uint256>& requests, std::shared_ptr<Peer> requestFrom);

    /**
     * TODO:
     * Removes a verified ms hash from the downloading queue, and start another
     * round of batch sync when the downloading queue is empty.
     * Returns whether the hash is removed successfully.
     */
    bool UpdateDownloadingQueue(const uint256&);

    /** Delete the chain who loses in the race competition */
    void DeleteChain(ChainPtr);
};

extern DAGManager& DAG;

#endif // __SRC_DAG_MANAGER_H__
