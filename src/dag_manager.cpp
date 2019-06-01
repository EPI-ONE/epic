#include "dag_manager.h"
#include "caterpillar.h"

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

ChainStatePtr DAGManager::GetState(const uint256& msHash) {
    auto search = globalStates_.find(msHash);
    if (search != globalStates_.end()) {
        return search->second; 
    } else if (auto prec = CAT->GetRecord(msHash)){
        return prec->snapshot; 
    } else {
        // cannot happen
        return nullptr; 
    }
}
