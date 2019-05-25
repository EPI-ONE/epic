#include "dag_manager.h"
#include "caterpillar.h"

DAGManager::DAGManager()
    : thread_(1), isBatchSynching(false), syncingPeer(nullptr), isVerifying(false), milestoneChains() {
    milestoneChains.push(std::make_unique<Chain>(true));
    thread_.Start();
}

DAGManager::~DAGManager() {
    thread_.Stop();
}

void DAGManager::RequestInv(const uint256&, const size_t&, std::shared_ptr<Peer>) {}

void DAGManager::AddBlockToPending(const ConstBlockPtr& block) {
    static auto process = [&block, this](const ChainPtr& chain) {
        chain->Verify(block);
        UpdateDownloadingQueue(block->GetHash());
    };

    thread_.Execute([block, this]() {
        // Extract utxos from outputs and pass their pointers to chains
        std::vector<UTXOPtr> utxos;
        if (block->HasTransaction()) {
            auto& outs = block->GetTransaction()->GetOutputs();
            utxos.reserve(outs.size());
            for (size_t i = 0; i < outs.size(); ++i) {
                utxos.push_back(std::make_shared<UTXO>(outs[i], i));
            }
        }

        for (auto chainIt = milestoneChains.begin(); chainIt != milestoneChains.end(); ++chainIt) {
            if ((*chainIt)->GetPendingBlockCount() > 0 && *block == GENESIS) {
                continue;
            }

            MilestoneStatus mss = (*chainIt)->AddPendingBlock(block);
            (*chainIt)->AddPendingUTXOs(utxos);

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
    });
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

void DAGManager::Stop() {
    while (thread_.GetTaskSize() > 0) {
        std::this_thread::yield();
    }
    thread_.Stop();
}
