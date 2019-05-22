#ifndef __SRC_DAG_MANAGER_H__
#define __SRC_DAG_MANAGER_H__

#include <atomic>
#include <list>

#include "chain.h"
#include "task.h"
#include "uint256.h"

class Peer;
class DAGManager {
public:
    static const std::unique_ptr<DAGManager>& GetDAGManager() {
        static const auto DAG = std::make_unique<DAGManager>();
        return DAG;
    }

    // TODO: For test only. Remove this in the future
    //       when AddBlockToPending becomes real
    std::vector<ConstBlockPtr> pending;

    // A list of hashes we've sent out in GetData requests.
    // It can be operated by multi threads.
    std::vector<uint256> downloading;

    // A list of tasks we've prepared to deliver to a Peer.
    // Operated by multi threads.
    std::vector<GetDataTask> preDownloading;

    // A list of milestone chains, with first element being the main chain
    // and others being forked chains.
    std::priority_queue<ChainPtr> milestoneChains;

    // Indicator of whether we are synching with some peer.
    // Needs atomic set and get operations since is accessible by multiple threads.
    std::atomic<bool> isBatchSynching;

    // The peer we are synching with. Null if isBatchSynching is false.
    std::shared_ptr<Peer> syncingPeer;

    DAGManager();

    // Called by Cat when a coming block is not solid. Do nothing if isBatchSynching
    // and syncingPeer is not null. This adds the GetBlocksMessage to Peer’s message
    // sending queue according to the BlockLocator constructed by peer.
    void RequestInv(const uint256& fromHash, const size_t& len, std::shared_ptr<Peer> peer);

    // Called by Peer and sets a list of inventory as the callback to the task.
    void AssembleInv(GetBlockTask& task);

    // Called by batchSync to create a GetDataTask for a given hash.
    GetDataTask RequestData(const uint256& hash, GetDataTask::Type& type, std::shared_ptr<Peer> peer);

    // Called by Peer and sets a Bundle as the callback to the task.
    void GetBundle(GetDataTask& task);

    // Adds a newly received block that has past syntax checking to the corresponding chain.
    void AddBlockToPending(const ConstBlockPtr& block);

    size_t GetBestMilestoneHeight();

    // Methods are called when the synchronization status is changed:
    // on to off and off to on. Modifies the atomic_flag isBatchSynching.
    void StartBatchSync(std::shared_ptr<Peer> peer);
    void CompleteBatchSync();

private:
    // Start a new thread and create a list of GetData tasks that is either added
    // to preDownloading (if it's not empty) or a peer's task queue. If preDownloading
    // is not empty, drain certain amount of tasks from preDownloading to peer's task queue.
    // Whenever a task is sent to peer, add the hash of the task in the downloading list.
    void BatchSync(std::list<uint256>& requests, std::shared_ptr<Peer> requestFrom);

    // Removes a verified ms hash from the downloading queue, and start another
    // round of batch sync when the downloading queue is empty.
    // Returns whether the hash is removed successfully.
    bool UpdateDownloadingQueue(uint256& hash);

    // Delete the chain who loses in the race competition
    void DeleteChain(ChainPtr);
};

static const auto& DAG = DAGManager::GetDAGManager();

#endif // __SRC_DAG_MANAGER_H__
