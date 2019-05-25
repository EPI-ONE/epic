#include "dag_manager.h"
#include "caterpillar.h"

DAGManager::DAGManager() : isBatchSynching(false), syncingPeer(nullptr), isVerifying(false), milestoneChains() {
    milestoneChains.push(std::make_unique<Chain>(true));
}

void DAGManager::RequestInv(const uint256&, const size_t&, std::shared_ptr<Peer>) {}

void DAGManager::AddBlockToPending(const ConstBlockPtr& block) {
    static auto process = [&block, this](const ChainPtr& chain) {
        auto lvs = chain->GetSortedSubgraph(block);
        chain->Verify(lvs);
        chain->UpdateChainState(lvs);
        UpdateDownloadingQueue(block->GetHash());
    };

    for (auto chainIt = milestoneChains.begin(); chainIt != milestoneChains.end(); ++chainIt) {
        if ((*chainIt)->GetPendingBlockCount() > 0 && *block == GENESIS) {
            continue;
        }

        MilestoneStatus mss = (*chainIt)->AddPendingBlock(block);

        switch (mss) {
        case IS_NOT_MILESTONE:
            continue;

        case IS_TRUE_MILESTONE: {
            isVerifying = true;
            process(*chainIt);
            milestoneChains.update_best(chainIt);
            isVerifying = false;
            break;
        }

        case IS_FAKE_MILESTONE: {
            // Add a new fork
            auto new_fork = std::make_unique<Chain>(**chainIt, block);
            process(new_fork);
            milestoneChains.emplace(std::move(new_fork));
            break;
        }
        }
    }
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

bool DAGManager::UpdateDownloadingQueue(const uint256&) {
    return false;
}

const Chain& DAGManager::GetBestChain() const {
    return *milestoneChains.best();
}

DAGManager& DAG = DAGManager::GetDAGManager();
