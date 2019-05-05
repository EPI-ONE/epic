# include "dag_manager.h"

DAGManager::DAGManager() {
    syncingPeer = nullptr;
}

void DAGManager::RequestInv(const uint256& fromHash, const size_t& len, const Peer& peer) {}

void DAGManager::AddBlockToPending(const BlockPtr& block) {}
