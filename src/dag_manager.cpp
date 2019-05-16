#include "dag_manager.h"

DAGManager::DAGManager() {
    syncingPeer = nullptr;
    isBatchSynching = false;
}

void DAGManager::RequestInv(const uint256& fromHash, const size_t& len, const Peer* peer) {}

void DAGManager::AddBlockToPending(const ConstBlockPtr& block) {
    // TODO: For test only!
    //       MUST be changed in the future.
    pending.push_back(block);
}
