#include "dag_manager.h"
#include "caterpillar.h"
#include "peer_manager.h"

DAGManager::DAGManager() : verifyThread_(1), syncPool_(1), storagePool_(1), isStoring(false), isVerifying(false) {
    milestoneChains.push(std::make_unique<Chain>());
    globalStates_.emplace(GENESIS.GetHash(), std::make_shared<NodeRecord>(GENESIS_RECORD));

    // Start threadpools
    verifyThread_.Start();
    syncPool_.Start();
    storagePool_.Start();
}

DAGManager::~DAGManager() {
    Stop();
}

bool DAGManager::Init() {
    // dag should have only one chain when calling Init()
    return milestoneChains.size() == 1;
}

/////////////////////////////////////
// Synchronization specific methods
//

void DAGManager::RequestInv(uint256 fromHash, const size_t& length, PeerPtr peer) {
    syncPool_.Execute([peer = std::move(peer), fromHash = std::move(fromHash), length, this]() {
        std::vector<uint256> locator = ConstructLocator(fromHash, length, peer);
        if (locator.empty()) {
            spdlog::debug("RequestInv return: locator is null");
            return;
        }

        peer->SetLastGetInvEnd(fromHash);
        peer->SetLastGetInvLength(length);

        auto task = std::make_shared<GetInvTask>(sync_task_timeout);
        peer->AddPendingGetInvTask(task);
        peer->SendMessage(std::make_unique<GetInv>(locator, task->nonce));
    });
}

void DAGManager::CallbackRequestInv(std::unique_ptr<Inv> inv, PeerPtr peer) {
    syncPool_.Execute([inv = std::move(inv), peer, this]() {
        auto& result = inv->hashes;
        if (result.empty()) {
            spdlog::info("Received an empty inv, which means we have reached the same height as the peer's {}.",
                         peer->address.ToString());
            auto task = std::make_shared<GetDataTask>(GetDataTask::PENDING_SET, sync_task_timeout);
            peer->AddPendingGetDataTask(task);
            auto pending_request = std::make_unique<GetData>(task->type);
            pending_request->AddPendingSetNonce(task->nonce);
            peer->SendMessage(std::move(pending_request));
            peer->isHigher = false;
        } else if (result.size() == 1 && result.at(0) == GENESIS.GetHash()) {
            RequestInv(peer->GetLastGetInvEnd(), 2 * peer->GetLastGetInvLength(), peer);
            spdlog::debug("We are probably on a fork... sending a larger locator.");
        } else {
            RequestData(result, peer);
        }
    });
}

void DAGManager::RespondRequestInv(std::vector<uint256>& locator, uint32_t nonce, PeerPtr peer) {
    syncPool_.Execute([peer = std::move(peer), locator = std::move(locator), nonce, this]() {
        std::vector<uint256> hashes;
        for (const uint256& start : locator) {
            if (start == GetMilestoneHead()->cblock->GetHash()) {
                // The peer already reach our head. Send an empty inv.
                spdlog::debug("The peer should already reach our head. Sending empty inv. "
                              "Last bundle sent to this peer: {}",
                              std::to_string(peer->GetLastSentBundleHash()));
                std::vector<uint256> empty;
                peer->SendMessage(std::make_unique<Inv>(empty, nonce));
                return;
            }
            if (IsMainChainMS(start)) {
                auto startMs = GetState(start);
                // This locator intersects with our database. We now have a starting point.
                // Traverse the milestone chain forward from the starting point itself.
                spdlog::debug("Constructing inv... Found a starting point of height {}", startMs->snapshot->height);
                hashes = TraverseMilestoneForward(*startMs, Inv::kMaxInventorySize);
                break;
            }
        }

        if (hashes.empty()) {
            // Cannot locate the peer's position. Send a genesis hash.
            hashes.push_back(GENESIS.GetHash());
        } else {
            uint256 lih = peer->GetLastSentInvHash();
            uint256 lbh = peer->GetLastSentBundleHash();

            // First, find the if the hashes to be sent contain the most recent
            // ms hash that we have sent to the peer via either Inv or Bundle,
            // to avoid duplicated GetData requests.
            auto it = std::find(hashes.begin(), hashes.end(), lih);
            if (it == hashes.end()) {
                lih.SetNull();
                it = std::find(hashes.begin(), hashes.end(), lbh);
            }

            // If found, remove all the hashes come before it,
            // and only keep the sublist starting from its next
            if (it != hashes.end()) {
                std::vector<uint256> sublist(it + 1, hashes.end());
                hashes.swap(sublist);
                sublist.clear();
            }

            if (hashes.empty()) {
                spdlog::debug("Sublist of inv is empty. Sending empty inv. "
                              "Last {} sent to this peer: {}",
                              lih.IsNull() ? "bundle" : "inv",
                              lih.IsNull() ? std::to_string(lbh) : std::to_string(lih));
            } else {
                peer->SetLastSentInvHash(hashes.back());
            }
        }

        peer->SendMessage(std::make_unique<Inv>(hashes, nonce));
    });
}

void DAGManager::RespondRequestPending(uint32_t nonce, const PeerPtr& peer) const {
    peer->SendMessage(std::make_unique<Bundle>(GetBestChain().GetPendingBlocks(), nonce));
}

void DAGManager::RespondRequestLVS(const std::vector<uint256>& hashes,
                                   const std::vector<uint32_t>& nonces,
                                   PeerPtr peer) {
    assert(hashes.size() == nonces.size());
    auto hs_iter = hashes.begin();
    auto nc_iter = nonces.begin();
    while (hs_iter != hashes.end() && nc_iter != nonces.end()) {
        syncPool_.Execute([n = *nc_iter, h = *hs_iter, peer, this]() {
            auto bundle  = std::make_unique<Bundle>(n);
            auto payload = GetMainChainRawLevelSet(h);
            if (payload.empty()) {
                spdlog::debug("Milestone {} cannot be found. Sending a Not Found Message instead", h.to_substr());
                peer->SendMessage(std::make_unique<NotFound>(h, n));
                return;
            }

            bundle->SetPayload(std::move(payload));
            spdlog::debug("Sending bundle of LVS with nonce {} with MS hash {} to peer {}", n, h.to_substr(),
                          peer->address.ToString());
            peer->SetLastSentBundleHash(h);
            peer->SendMessage(std::move(bundle));
        });
        hs_iter++;
        nc_iter++;
    }
}

void DAGManager::RequestData(std::vector<uint256>& requests, const PeerPtr& requestFrom) {
    auto message = std::make_unique<GetData>(GetDataTask::LEVEL_SET);
    for (auto& h : requests) {
        if (downloading.contains(h) || CAT->Exists(h)) {
            continue;
        }

        auto task = std::make_shared<GetDataTask>(GetDataTask::LEVEL_SET, h, sync_task_timeout);
        message->AddItem(h, task->nonce);
        requestFrom->AddPendingGetDataTask(task);
        downloading.insert(h);

        if (message->hashes.size() >= maxGetDataSize) {
            requestFrom->SendMessage(std::move(message));
            message = std::make_unique<GetData>(GetDataTask::LEVEL_SET);
        }
    }

    if (!message->hashes.empty()) {
        requestFrom->SendMessage(std::move(message));
    }
}

std::vector<uint256> DAGManager::ConstructLocator(const uint256& fromHash, size_t length, const PeerPtr& peer) {
    const RecordPtr startMilestone = fromHash.IsNull() ? GetMilestoneHead() : GetState(fromHash);
    if (!startMilestone) {
        return {};
    }

    return TraverseMilestoneBackward(*startMilestone, length);
}

std::vector<uint256> DAGManager::TraverseMilestoneBackward(const NodeRecord& cursor, size_t length) const {
    std::vector<uint256> result;
    result.reserve(length);

    const auto& bestChain    = milestoneChains.best();
    size_t leastHeightCached = bestChain->GetLeastHeightCached();
    size_t cursorHeight      = GetHeight(cursor.cblock->GetHash());

    if (cursorHeight > GetBestMilestoneHeight()) {
        return result;
    }

    // If the cursor height is within the cache range,
    // traverse the best chain cache.
    while (cursorHeight >= leastHeightCached && result.size() < length) {
        result.push_back(bestChain->GetStates()[cursorHeight - leastHeightCached]->GetMilestoneHash());
        --cursorHeight;
    }

    // If we have reached the least height in cache and still don't get enough hashes,
    // continue to traverse DB until we reach the capacity.
    while (result.size() < length) {
        if (cursorHeight == 0) {
            result.push_back(GENESIS.GetHash());
            break;
        }
        result.push_back(CAT->GetMilestoneAt(cursorHeight)->cblock->GetHash());
        --cursorHeight;
    }

    return result;
}

std::vector<uint256> DAGManager::TraverseMilestoneForward(const NodeRecord& cursor, size_t length) const {
    std::vector<uint256> result;
    result.reserve(length);

    const auto& bestChain    = milestoneChains.best();
    size_t leastHeightCached = bestChain->GetLeastHeightCached();
    size_t cursorHeight      = GetHeight(cursor.cblock->GetHash()) + 1;

    if (cursorHeight == 0) {
        return result;
    }

    // If the cursor height is less than the least height in cache, traverse DB.
    while (cursorHeight <= CAT->GetHeadHeight() && result.size() < length) {
        result.push_back(CAT->GetMilestoneAt(cursorHeight)->cblock->GetHash());
        ++cursorHeight;
    }

    // If we have reached the hightest milestone in DB and still don't get enough hashes,
    // continue to traverse the best chain cache until we reach the head or the capacity.
    while (result.size() < length && leastHeightCached != UINT64_MAX &&
           cursorHeight < leastHeightCached + bestChain->GetStates().size()) {
        result.push_back(bestChain->GetStates()[cursorHeight - leastHeightCached]->GetMilestoneHash());
        ++cursorHeight;
    }

    return result;
}

bool DAGManager::UpdateDownloadingQueue(const uint256& hash) {
    auto size     = downloading.size();
    bool contains = bool(downloading.erase(hash));
    if (contains) {
        spdlog::debug("Size of downloading = {}, removed successfully", size - 1);
    } else if (!downloading.empty()) {
        spdlog::debug("Update downloading failed when processing the ms {}", std::to_string(hash));
        for (auto it = downloading.begin(); it != downloading.end();) {
            if (ExistsNode(*it)) {
                spdlog::trace("Removing existing {} from downloading.", std::to_string(*it));
                it = downloading.erase(it);
            } else {
                spdlog::trace("Remaining hash {} in downloading.", std::to_string(*it));
                ++it;
            }
        }
    }

    time_t now      = time(nullptr);
    time_t headTime = GetMilestoneHead()->cblock->GetTime();
    if (now - headTime < obcEnableThreshold) {
        CAT->EnableOBC();
    }

    return contains;
}

/////////////////////////////////////
// End of synchronization methods
//
void DAGManager::AddNewBlock(ConstBlockPtr blk, PeerPtr peer) {
    verifyThread_.Execute([=, blk = std::move(blk), peer = std::move(peer)]() mutable {
        if (*blk == GENESIS) {
            return;
        }

        if (CAT->Exists(blk->GetHash())) {
            return;
        }

        /////////////////////////////////
        // Start of online verification

        if (!blk->Verify()) {
            return;
        }

        // Check solidity ///////////////
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
            RecordPtr ms = GetState(msHash, false);
            if (ms && !CheckPuntuality(blk, ms)) {
                return;
            }
            // Abort and send GetBlock requests.
            CAT->AddBlockToOBC(blk, mask());

            if (peer) {
                peer->StartSync();
            }

            return;
        }

        // Check difficulty target //////

        RecordPtr ms = GetState(msHash, false);
        if (!ms) {
            spdlog::info("Block has missing or invalid milestone link [{}]", std::to_string(blk->GetHash()));
            return;
        }

        uint32_t expectedTarget = ms->snapshot->blockTarget.GetCompact();
        if (blk->GetDifficultyTarget() != expectedTarget) {
            spdlog::info("Block has unexpected change in difficulty: current {} v.s. expected {} [{}]",
                         blk->GetDifficultyTarget(), expectedTarget, std::to_string(blk->GetHash()));
            return;
        }

        // Check punctuality ////////////

        if (!CheckPuntuality(blk, ms)) {
            return;
        }

        // End of online verification
        /////////////////////////////////

        CAT->Cache(blk);

        if (peer) {
            PEERMAN->RelayBlock(blk, peer);
        }

        AddBlockToPending(blk);
        CAT->ReleaseBlocks(blk->GetHash());
    });
}

bool DAGManager::CheckPuntuality(const ConstBlockPtr& blk, const RecordPtr& ms) const {
    assert(ms);

    if (blk->IsFirstRegistration()) {
        return true;
    }

    if (blk->GetMilestoneHash() == GENESIS.GetHash()) {
        return true;
    }

    assert(milestoneChains.size() > 0);
    assert(milestoneChains.best());

    auto bestHeight = GetBestMilestoneHeight();
    if (bestHeight > ms->height && (bestHeight - ms->height) > GetParams().cacheStatesSize) {
        spdlog::info("Block is too old: pointing to height {} vs. current head height {} [{}]", ms->height, bestHeight,
                     std::to_string(blk->GetHash()));
        return false;
    }

    return true;
}

void DAGManager::AddBlockToPending(const ConstBlockPtr& block) {
    spdlog::trace("AddBlockToPending {}", block->GetHash().to_substr());

    // Extract utxos from outputs and pass their pointers to chains
    std::vector<UTXOPtr> utxos;
    const auto& txns = block->GetTransactions();
    for (size_t i = 0; i < txns.size(); ++i) {
        auto& outs = txns[i]->GetOutputs();
        utxos.reserve(outs.size());
        for (size_t j = 0; j < outs.size(); ++j) {
            utxos.emplace_back(std::make_shared<UTXO>(outs[j], i, j));
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
        if (CheckMsPOW(block, ms)) {
            if (*msBlock->cblock == *GetMilestoneHead()->cblock) {
                // new milestone on mainchain
                ProcessMilestone(mainchain, block);
                if (!isStoring.load()) {
                    if (size_t level = FlushTrigger()) {
                        isStoring = true;
                        DeleteFork();
                        FlushToCAT(level);
                    }
                }
            } else {
                // new fork
                spdlog::debug("A fork created which points to MS block {}", block->GetHash().to_substr());
                auto new_fork = std::make_unique<Chain>(*milestoneChains.best(), block);
                ProcessMilestone(new_fork, block);
                milestoneChains.emplace(std::move(new_fork));
            }
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

        if (CheckMsPOW(block, ms)) {
            if (msBlock->cblock->GetHash() == chain->GetChainHead()->GetMilestoneHash()) {
                // new milestone on fork
                spdlog::debug("A fork grows to height {}", (*chainIt)->GetChainHead()->height);
                ProcessMilestone(*chainIt, block);
                milestoneChains.update_best(chainIt);
                return;
            } else {
                // new fork
                spdlog::debug("A fork created which points to MS block {}", block->GetHash().to_substr());
                auto new_fork = std::make_unique<Chain>(*chain, block);
                ProcessMilestone(new_fork, block);
                milestoneChains.emplace(std::move(new_fork));
                return;
            }
        }
    }
}

void DAGManager::ProcessMilestone(const ChainPtr& chain, const ConstBlockPtr& block) {
    isVerifying = true;

    auto newMs = chain->Verify(block);
    globalStates_.emplace(block->GetHash(), newMs);
    chain->AddNewState(*newMs);

    isVerifying = false;

    UpdateDownloadingQueue(block->GetHash());
}

void DAGManager::DeleteFork() {
    const auto& states   = GetBestChain().GetStates();
    auto targetChainWork = (*(states.end() - GetParams().deleteForkThreshold))->chainwork;
    for (auto chain_it = milestoneChains.begin(); chain_it != milestoneChains.end();) {
        if (*chain_it == milestoneChains.best()) {
            chain_it++;
            continue;
        }
        // try delete
        if ((*chain_it)->GetChainHead()->chainwork < targetChainWork) {
            chain_it = milestoneChains.erase(chain_it);
        } else {
            chain_it++;
        }
    }
}

RecordPtr DAGManager::GetState(const uint256& msHash, bool withBlock) const {
    auto search = globalStates_.find(msHash);
    if (search != globalStates_.end()) {
        return search->second;
    }

    auto prec = CAT->GetRecord(msHash, withBlock);
    if (prec && prec->snapshot) {
        return prec;
    }

    // cannot happen for in-dag workflow
    // may return nullptr when rpc is requesting some non-existing states
    return nullptr;
}

Chain& DAGManager::GetBestChain() const {
    return *milestoneChains.best();
}

void DAGManager::Stop() {
    Wait();
    syncPool_.Stop();
    verifyThread_.Stop();
    storagePool_.Stop();
}

void DAGManager::Wait() {
    while (!verifyThread_.IsIdle() || !storagePool_.IsIdle() || !syncPool_.IsIdle()) {
        std::this_thread::yield();
    }
}

size_t DAGManager::FlushTrigger() {
    const auto& bestChain = milestoneChains.best();
    if (bestChain->GetStates().size() < GetParams().cacheStatesSize) {
        return 0;
    }
    std::vector<ConcurrentQueue<ChainStatePtr>::const_iterator> forks;
    forks.reserve(milestoneChains.size() - 1);
    for (auto& chain : milestoneChains) {
        if (chain == bestChain) {
            continue;
        }
        forks.emplace_back(chain->GetStates().begin());
    }

    auto cursor   = bestChain->GetStates().begin();
    size_t dupcnt = 0;
    for (bool flag = true; flag && dupcnt < GetParams().cacheStatesToDelete; dupcnt++) {
        for (auto& fork_it : forks) {
            if (*cursor != *fork_it) {
                flag = false;
                break;
            }
            fork_it++;
        }
        cursor++;
    }

    return dupcnt;
}

void DAGManager::FlushToCAT(size_t level) {
    // first store data to CAT
    auto [recToStore, utxoToStore, utxoToRemove] = milestoneChains.best()->GetDataToCAT(level);

    spdlog::trace("flushing at current height {}", GetBestMilestoneHeight());

    storagePool_.Execute([=, recToStore = std::move(recToStore), utxoToStore = std::move(utxoToStore),
                          utxoToRemove = std::move(utxoToRemove)]() mutable {
        std::vector<RecordPtr> blocksToListener;
        blocksToListener.reserve(recToStore.size());

        for (auto& lvsRec : recToStore) {
            std::swap(lvsRec.front(), lvsRec.back());
            const auto& ms = *lvsRec.front().lock();

            CAT->StoreLevelSet(lvsRec);
            CAT->UpdatePrevRedemHashes(ms.snapshot->regChange);

            std::swap(lvsRec.front(), lvsRec.back());
            for (auto& rec : lvsRec) {
                blocksToListener.emplace_back(rec.lock());
                CAT->UnCache((*rec.lock()).cblock->GetHash());
            }
            globalStates_.erase(ms.cblock->GetHash());
        }
        for (const auto& [utxoKey, utxoPtr] : utxoToStore) {
            CAT->AddUTXO(utxoKey, utxoPtr);
        }
        for (const auto& utxoKey : utxoToRemove) {
            CAT->RemoveUTXO(utxoKey);
        }

        // notify the listener
        if (onLvsConfirmedCallback) {
            onLvsConfirmedCallback(std::move(blocksToListener), utxoToStore, utxoToRemove);
        }

        // then remove chain states from chains
        size_t totalRecNum = 0;
        std::for_each(recToStore.begin(), recToStore.end(),
                      [&totalRecNum](const auto& lvs) { totalRecNum += lvs.size(); });
        std::vector<uint256> recHashes{};
        recHashes.reserve(totalRecNum);
        std::unordered_set<uint256> utxoCreated{};
        utxoCreated.reserve(utxoToStore.size());

        for (auto& lvsRec : recToStore) {
            for (auto& rec : lvsRec) {
                recHashes.emplace_back((*rec.lock()).cblock->GetHash());
            }
        }

        for (const auto& [key, value] : utxoToStore) {
            utxoCreated.emplace(key);
        }

        TXOC txocToRemove{std::move(utxoCreated), std::move(utxoToRemove)};

        verifyThread_.Execute(
            [=, recHashes = std::move(recHashes), txocToRemove = std::move(txocToRemove), level = level]() {
                for (auto& chain : milestoneChains) {
                    chain->PopOldest(recHashes, txocToRemove, level);
                }
                isStoring = false;
            });
    });
}

bool CheckMsPOW(const ConstBlockPtr& b, const ChainStatePtr& m) {
    return !(UintToArith256(b->GetHash()) > m->milestoneTarget);
}

RecordPtr DAGManager::GetMilestoneHead() const {
    if (GetBestChain().GetStates().empty()) {
        return CAT->GetMilestoneAt(CAT->GetHeadHeight());
    }

    return GetBestChain().GetChainHead()->GetMilestone();
}

size_t DAGManager::GetBestMilestoneHeight() const {
    if (GetBestChain().GetStates().empty()) {
        return CAT->GetHeadHeight();
    }
    return GetBestChain().GetChainHead()->height;
}

bool DAGManager::IsMainChainMS(const uint256& blkHash) const {
    return GetBestChain().IsMilestone(blkHash);
}

RecordPtr DAGManager::GetMainChainRecord(const uint256& blkHash) const {
    auto rec = GetBestChain().GetRecord(blkHash);
    if (!rec) {
        rec = CAT->GetRecord(blkHash);
    }
    return rec;
}

size_t DAGManager::GetHeight(const uint256& blockHash) const {
    auto rec = GetBestChain().GetRecordCache(blockHash);
    if (rec) {
        return rec->height;
    }
    return CAT->GetHeight(blockHash);
}

std::vector<ConstBlockPtr> DAGManager::GetMainChainLevelSet(size_t height) const {
    std::vector<ConstBlockPtr> lvs;
    const auto& bestChain    = GetBestChain();
    size_t leastHeightCached = bestChain.GetLeastHeightCached();

    if (height < leastHeightCached) {
        auto recs = CAT->GetLevelSetBlksAt(height);
        lvs.insert(lvs.end(), std::make_move_iterator(recs.begin()), std::make_move_iterator(recs.end()));
    } else {
        auto recs = bestChain.GetStates()[height - leastHeightCached]->GetLevelSet();
        std::swap(recs.front(), recs.back());

        lvs.reserve(recs.size());
        for (auto& rwp : recs) {
            lvs.push_back((*rwp.lock()).cblock);
        }
    }

    return lvs;
}

std::vector<ConstBlockPtr> DAGManager::GetMainChainLevelSet(const uint256& blockHash) const {
    return GetMainChainLevelSet(GetHeight(blockHash));
}

std::vector<RecordPtr> DAGManager::GetLevelSet(const uint256& blockHash, bool withBlock) const {
    std::vector<RecordPtr> lvs;
    size_t leastHeightCached = GetBestChain().GetLeastHeightCached();

    auto height = GetHeight(blockHash);
    if (height < leastHeightCached) {
        auto recs = CAT->GetLevelSetRecsAt(height, withBlock);
        lvs.insert(lvs.end(), std::make_move_iterator(recs.begin()), std::make_move_iterator(recs.end()));
    } else {
        auto state = GetState(blockHash);
        if (state) {
            auto recs = state->snapshot->GetLevelSet();
            lvs.reserve(recs.size());
            for (auto& rwp : recs) {
                lvs.push_back(rwp.lock());
            }
        }
    }

    return lvs;
}

VStream DAGManager::GetMainChainRawLevelSet(size_t height) const {
    const auto& bestChain    = GetBestChain();
    size_t leastHeightCached = bestChain.GetLeastHeightCached();

    // Find in DB
    if (height < leastHeightCached) {
        return CAT->GetRawLevelSetAt(height);
    }

    // Find in cache
    auto recs = bestChain.GetStates()[height - leastHeightCached]->GetLevelSet();

    // To make it have the same order as the lvs we get from file (ms goes the first)
    std::swap(recs.front(), recs.back());

    VStream result;
    for (auto& rwp : recs) {
        result << (*rwp.lock()).cblock;
    }

    return result;
}

VStream DAGManager::GetMainChainRawLevelSet(const uint256& blockHash) const {
    return GetMainChainRawLevelSet(GetHeight(blockHash));
}

bool DAGManager::ExistsNode(const uint256& h) const {
    for (const auto& chain : milestoneChains) {
        if (chain->GetRecord(h)) {
            return true;
        }
    }
    return false;
}

void DAGManager::RegisterOnLvsConfirmedCallback(OnLvsConfirmedCallback&& callback_func) {
    onLvsConfirmedCallback = std::move(callback_func);
}
