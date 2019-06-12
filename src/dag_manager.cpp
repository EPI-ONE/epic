#include "dag_manager.h"
#include "caterpillar.h"

DAGManager::DAGManager() {
    syncingPeer     = nullptr;
    isBatchSynching = false;
}

void DAGManager::RequestInv(const uint256&, const size_t&, std::shared_ptr<Peer>) {}

void DAGManager::AddBlockToPending(const ConstBlockPtr& block) {
    // TODO: For test only!
    //       MUST be changed in the future.
    pending.push_back(block);
}

RecordPtr DAGManager::GetState(const uint256& msHash) {
    auto search = globalStates_.find(msHash);
    if (search != globalStates_.end()) {
        return search->second;
    } else {
        auto prec = CAT->GetRecord(msHash);
        if (prec && prec->snapshot) {
            return prec;

        } else {
            // cannot happen
            return nullptr;
        }
    }
}
