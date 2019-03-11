#ifndef __SRC_DAG_MANAGER_H__
#define __SRC_DAG_MANAGER_H__

#include<list>

#include<utils/uint256.h>
#include<chain.h>
#include<task.h>

class Block;
class GetBlockTask;
class GetDataTask;
class Peer;
class Type;

class DAGManager {

    public:
        // A list of hashes we've sent out in GetData requests.
        // Operated by multi threads.
        std::list<uint256> downloading;

        // A list of tasks we've prepared to deliver to a Peer.
        // Operated by multi threads.
        std::list<Task> preDownloading;

        // A list of milestone chains, with index 0 being the main chain
        // and others being forked chains.
        std::list<Chain> milestoneChains;

        // Indicator of whether we are synching with some peer.
        // Needs atomic set and get operations since is accessible by multiple threads.
        bool isBatchSynching;

        // The peer we are synching with. Null if isBatchSynching is false.
        //Peer& syncingPeer;

        DAGManager();

        void init();

        // Called by Cat when a block is found not solid. Do nothing if isBatchSynching
        // and syncingPeer is not null. This adds the GetBlocksMessage to Peer’s message
        // sending queue according to the BlockLocator constructed by peer.
        void requestInv(uint256 fromHash, uint32_t length, Peer peer);

        // Called by Peer and sets a list of inventory as the callback to the task.
        void assembleInv(GetBlockTask& task);

        // Called by batchSync to create a GetDataTask for a given hash.
        GetDataTask requestData(uint256 hash, Type type, Peer& peer);

        // Called by Peer and sets a Bundle as the callback to the task.
        void getBundle(GetDataTask task);

        // Adds a newly received block that has past syntax checking to the corresponding chain.
        void addBlockToPending();

        // Topological sort the blocks that have past syntax checking to the corresponding chain.
        void addBlocksToPending();

        uint32_t getBestMilestoneHeight();

        // Guarded by lock
        void startBatchSync(Peer& peer) {
            if (isBatchSynching == false) {
                isBatchSynching = true;
                //syncingPeer = peer;
            }
        }

        // Guarded by lock
        void completeBatchSync() {
            if (isBatchSynching == true) {
                isBatchSynching = false;
                //syncingPeer = NULL;
            }
        }

    private:
        // Start a new thread and create a list of GetData tasks that is either added
        // to preDownloading (if it's not empty) or a peer's task queue. If preDownloading
        // is not empty, drain certain amount of tasks from preDownloading to peer's task queue.
        // Whenever a task is sent to peer, add the hash of the task in the downloading list.
        void batchSync(std::list<uint256>& requests, Peer& requestFrom);

        // Removes a verified ms hash from the downloading queue, and start another
        // round of batch sync when the downloading queue is empty.
        // Returns whether the hash is removed successfully.
        bool updateDownloadingQueue(uint256 hash);

        // Swaps a forked chain with the main chain if the forked chain wins a competition.
        // Rolls back and rebuild the ledger if neccessary.
        void switchChain();

        std::list<Block&> topologicalSort(const std::list<const Block&> graph);
};

#endif // __SRC_DAG_MANAGER_H__
