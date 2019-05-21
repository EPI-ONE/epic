#include "dag_manager.h"

DAGManager::DAGManager() {
    syncingPeer     = nullptr;
    isBatchSynching = false;
}

void DAGManager::RequestInv(const uint256&, const size_t&, std::shared_ptr<Peer> peer) {}

void DAGManager::AddBlockToPending(const ConstBlockPtr& block) {
    // TODO: For test only!
    //       MUST be changed in the future.
    pending.push_back(block);
}
