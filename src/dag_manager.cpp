#include "dag_manager.h"
#include "caterpillar.h"

DAGManager::DAGManager()
    : thread_(1), isBatchSynching(false), syncingPeer(nullptr), isVerifying(false), milestoneChains() {
    milestoneChains.push(std::make_unique<Chain>());
    thread_.Start();
    globalStates_.emplace(GENESIS.GetHash(), make_shared_ChainState());
}

DAGManager::~DAGManager() {
    thread_.Stop();
}

void DAGManager::RequestInv(const uint256&, const size_t&, std::shared_ptr<Peer>) {}

void DAGManager::AddBlockToPending(const ConstBlockPtr& block) {
    static auto process = [&block, this](const ChainPtr& chain) {
        isVerifying = true;
        chain->Verify(block);
        UpdateDownloadingQueue(block->GetHash());
        isVerifying = false;
    };

    thread_.Execute([block, this]() {
        // Extract utxos from outputs and pass their pointers to chains
        std::vector<UTXOPtr> utxos;
        if (block->HasTransaction()) {
            auto& outs = block->GetTransaction()->GetOutputs();
            utxos.reserve(outs.size());
            for (size_t i = 0; i < outs.size(); ++i) {
                utxos.emplace_back(std::make_shared<UTXO>(outs[i], i));
            }
        }

        // Add to pending on every chain
        for (const auto& chain : milestoneChains) {
            chain->AddPendingBlock(block);
            chain->AddPendingUTXOs(utxos);
        }

        // Check if it's a new ms from the main chain
        auto& mainchain    = milestoneChains.best();
        const auto& msHash = block->GetMilestoneHash();
        RecordPtr msBlock  = mainchain->GetMsRecordCache(msHash);
        if (!msBlock) {
            msBlock = CAT->GetRecord(msHash);
        }

        if (msBlock) {
            auto ms = msBlock->snapshot;

            if (*(ms) == *(mainchain->GetChainHead()) && CheckMsPOW(block, ms)) {
                // new milestone
                process(mainchain);
            } else if (CheckMsPOW(block, ms)) {
                // new fork
                auto new_fork = std::make_unique<Chain>(*milestoneChains.best(), block);
                process(new_fork);
                milestoneChains.emplace(std::move(new_fork));
            }

            return;
        }

        // Check if it's a milestone on any other chain
        for (auto chainIt = milestoneChains.begin(); chainIt != milestoneChains.end(); ++chainIt) {
            ChainPtr& chain   = *chainIt;

            if (chain->IsMainChain()) {
                continue;
            }

            msBlock = chain->GetMsRecordCache(msHash);

            if (!msBlock) {
                continue;
            }

            ChainStatePtr ms = msBlock->snapshot;

            if (*(ms) == *(chain->GetChainHead()) && CheckMsPOW(block, ms)) {
                // new milestone
                process(*chainIt);
                milestoneChains.update_best(chainIt);
                return;
            } else if (CheckMsPOW(block, ms)) {
                // new fork
                auto new_fork = std::make_unique<Chain>(*chain, block);
                process(new_fork);
                milestoneChains.emplace(std::move(new_fork));
                return;
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

bool CheckMsPOW(const ConstBlockPtr& b, const ChainStatePtr& m) {
    return !(UintToArith256(b->GetHash()) > m->milestoneTarget);
}
