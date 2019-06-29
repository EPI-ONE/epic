#include "dag_manager.h"
#include "caterpillar.h"
#include "peer_manager.h"

DAGManager::DAGManager()
    : verifyThread_(1), isBatchSynching(false), syncingPeer(nullptr), isVerifying(false), milestoneChains() {
    milestoneChains.push(std::make_unique<Chain>());
    verifyThread_.Start();
    globalStates_.emplace(GENESIS.GetHash(), std::make_shared<NodeRecord>(GENESIS_RECORD));
}

DAGManager::~DAGManager() {
    verifyThread_.Stop();
}

void DAGManager::RequestInv(const uint256&, const size_t&, PeerPtr) {}

void DAGManager::AddNewBlock(const ConstBlockPtr& blk, PeerPtr peer) {
    verifyThread_.Execute([blk, peer, this]() {
        if (*blk == GENESIS) {
            return;
        }

        if (CAT->Exists(blk->GetHash())) {
            return;
        }

        //////////////////////////////////////////////////
        // Start of online verification

        if (!blk->Verify()) {
            return;
        }

        // Check solidity ////////////////////////////////

        const uint256& msHash   = blk->GetMilestoneHash();
        const uint256& prevHash = blk->GetPrevHash();
        const uint256& tipHash  = blk->GetTipHash();

        auto mask = [msHash, prevHash, tipHash]() {
            return ((!CAT->DAGExists(msHash) << 0) | (!CAT->DAGExists(prevHash) << 2) |
                    (!CAT->DAGExists(tipHash) << 1));
        };

        // First, check if we already received its preceding blocks
        if (CAT->IsWeaklySolid(blk)) {
            if (CAT->AnyLinkIsOrphan(blk)) {
                CAT->AddBlockToOBC(blk, mask());
                return;
            }
        } else {
            // We have not received at least one of its parents.

            // Drop if the block is too old
            RecordPtr ms = GetState(msHash);
            if (ms && !CheckPuntuality(blk, ms)) {
                return;
            }
            // Abort and send GetBlock requests.
            CAT->AddBlockToOBC(blk, mask());

            if (peer) {
                RequestInv(Hash::GetZeroHash(), 5, peer);
            }

            return;
        }

        // Check difficulty target ///////////////////////

        RecordPtr ms = GetState(msHash);
        if (!ms) {
            spdlog::info("Block has missing or invalid milestone link [{}]", std::to_string(blk->GetHash()));
            return;
        }

        uint32_t expectedTarget = ms->snapshot->blockTarget.GetCompact();
        if (blk->GetDifficultyTarget() != expectedTarget) {
            spdlog::info("Block has unexpected change in difficulty: current {} v.s. expected {} [{}]",
                         blk->GetDifficultyTarget(), expectedTarget);
            return;
        }

        // Check punctuality /////////////////////////////

        if (!CheckPuntuality(blk, ms)) {
            return;
        }

        // End of online verification
        //////////////////////////////////////////////////

        CAT->Cache(blk);

        // TODO: Relay block

        AddBlockToPending(blk);
        CAT->ReleaseBlocks(blk->GetHash());
    });
}

void DAGManager::AddBlockToPending(const ConstBlockPtr& block) {
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
            // new milestone on mainchain
            ProcessMilestone(mainchain, block);
        } else if (CheckMsPOW(block, ms)) {
            // new fork
            auto new_fork = std::make_unique<Chain>(*milestoneChains.best(), block);
            ProcessMilestone(new_fork, block);
            milestoneChains.emplace(std::move(new_fork));
        }

        return;
    }

    // Check if it's a milestone on any other chain
    for (auto chainIt = milestoneChains.begin(); chainIt != milestoneChains.end(); ++chainIt) {
        ChainPtr& chain = *chainIt;

        if (chain->IsMainChain()) {
            continue;
        }

        msBlock = chain->GetMsRecordCache(msHash);

        if (!msBlock) {
            continue;
        }

        ChainStatePtr ms = msBlock->snapshot;

        if (*(ms) == *(chain->GetChainHead()) && CheckMsPOW(block, ms)) {
            // new milestone on fork
            ProcessMilestone(*chainIt, block);
            milestoneChains.update_best(chainIt);
            return;
        } else if (CheckMsPOW(block, ms)) {
            // new fork
            auto new_fork = std::make_unique<Chain>(*chain, block);
            ProcessMilestone(new_fork, block);
            milestoneChains.emplace(std::move(new_fork));
            return;
        }
    }
}

void DAGManager::ProcessMilestone(const ChainPtr& chain, const ConstBlockPtr& block) {
    isVerifying = true;
    chain->Verify(block);
    UpdateDownloadingQueue(block->GetHash());
    isVerifying = false;
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

void DAGManager::Stop() {
    while (verifyThread_.GetTaskSize() > 0) {
        std::this_thread::yield();
    }
    verifyThread_.Stop();
}

bool CheckPuntuality(const ConstBlockPtr& blk, const RecordPtr& ms) {
    if (ms == nullptr) {
        // Should not happen
        return false;
    }

    if (blk->IsFirstRegistration()) {
        return true;
    }

    if (blk->GetMilestoneHash() == GENESIS.GetHash()) {
        return true;
    }

    if (blk->GetTime() - ms->cblock->GetTime() > GetParams().punctualityThred) {
        spdlog::info("Block is too old [{}]", std::to_string(blk->GetHash()));
        return false;
    }
    return true;
}

bool CheckMsPOW(const ConstBlockPtr& b, const ChainStatePtr& m) {
    return !(UintToArith256(b->GetHash()) > m->milestoneTarget);
}
