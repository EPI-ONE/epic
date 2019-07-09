#include "dag_manager.h"
#include "caterpillar.h"
#include "peer_manager.h"

DAGManager::DAGManager()
    : verifyThread_(1), syncPool_(1), storagePool_(2), isBatchSynching(false), syncingPeer(nullptr),
      isVerifying(false) {
    milestoneChains.push(std::make_unique<Chain>());
    globalStates_.emplace(GENESIS.GetHash(), std::make_shared<NodeRecord>(GENESIS_RECORD));
    verifyThread_.Start();
    syncPool_.Start();
    storagePool_.Start();
}

DAGManager::~DAGManager() {
    Stop();
}

/////////////////////////////////////
// Synchronization specific methods
//
#define SEND_MESSAGE(peer, type, msg) peer->SendMessage(NetMessage(peer->connection_handle, type, msg));

void DAGManager::RequestInv(const uint256& fromHash, const size_t& length, PeerPtr peer) {
    syncPool_.Execute([peer = std::move(peer), fromHash = std::move(fromHash), length, this]() {
        if (peer->isSeed) {
            return;
        }

        if (isBatchSynching.load() && peer != syncingPeer) {
            spdlog::debug("Sync ABORTED: syncing with peer {} which is not this peer {}",
                          syncingPeer->address.ToString(), peer->address.ToString());
            return;
        }

        std::vector<uint256> locator = ConstructLocator(fromHash, length, peer);
        if (locator.empty()) {
            spdlog::debug("RequestInv return: locator is null");
            return;
        }

        StartBatchSync(peer);

        if (!downloading.empty() || !preDownloading.Empty()) {
            downloading.clear();
            preDownloading.Clear();
        }

        GetInvTask task{};
        task.SetPeer(peer);
        GetInv message(locator, task.id);
        peer->AddPendingGetInvTask(task);
        SEND_MESSAGE(peer, GET_INV, message);
    });
}

void DAGManager::CallbackRequestInv(std::unique_ptr<Inv> inv) {
    syncPool_.Execute([inv = std::move(inv), this]() {
        auto& result = inv->hashes;
        if (result.empty()) {
            spdlog::info("Received an empty inv, which means we have reached the same height as the peer's.",
                         syncingPeer->address.ToString());
            GetDataTask task(GetDataTask::PENDING_SET);
            task.SetPeer(syncingPeer);
            syncingPeer->AddPendingGetDataTask(task);
            GetData pending_request(task.type);
            pending_request.AddPendingSetNonce(task.id);
            SEND_MESSAGE(syncingPeer, GET_DATA, pending_request);
            spdlog::trace("Synching peer to complete {}", syncingPeer->address.ToString());
            CompleteBatchSync();
        } else if (result.size() == 1 && result.at(0) == GENESIS.GetHash()) {
            RequestInv(syncingPeer->GetLastGetInvEnd(), 2 * syncingPeer->GetLastGetInvLength(), syncingPeer);
            spdlog::debug("We are probably on a fork... sending a larger locator.");
        } else {
            BatchSync(result, syncingPeer);
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
                SEND_MESSAGE(peer, INV, Inv(empty, nonce));
                return;
            }
            if (IsMainChainMS(start)) {
                auto startMs = GetState(start);
                // This locator intersects with our database. We now have a starting point.
                // Traverse the milestone chain forward from the starting point itself.
                spdlog::debug("Constructing inv... Found a starting point of height {}", startMs->snapshot->height);
                hashes = TraverseMilestoneForward(startMs, Inv::kMaxInventorySize);
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

        SEND_MESSAGE(peer, INV, Inv(hashes, nonce));
    });
}

std::optional<GetDataTask> DAGManager::RequestData(const uint256& h, const PeerPtr& peer) {
    if (downloading.contains(h)) {
        spdlog::debug("Duplicated GetData request: {}", std::to_string(h));
        return {};
    }

    if (CAT->Exists(h)) {
        spdlog::debug("ABORT GetData request for existing hash: {}", std::to_string(h));
        return {};
    }

    GetDataTask task(GetDataTask::LEVEL_SET);
    task.SetPeer(peer);
    return std::optional<GetDataTask>(task);
}

void DAGManager::CallbackRequestData(std::vector<ConstBlockPtr>& blocks) {
    spdlog::debug("Add {} blocks through CallbackRequestData.", blocks.size());
    for (auto& b : blocks) {
        AddNewBlock(b, syncingPeer);
    }
    blocks.clear();
}

void DAGManager::RespondRequestPending(uint32_t nonce, const PeerPtr& peer) {
    SEND_MESSAGE(peer, BUNDLE, Bundle(GetBestChain().GetPendingBlocks(), nonce));
}

void DAGManager::RespondRequestLVS(const std::vector<uint256>& hashes,
                                   const std::vector<uint32_t>& nonces,
                                   PeerPtr peer) {
    assert(hashes.size() == nonces.size());
    auto hs_iter = hashes.begin();
    auto nc_iter = nonces.begin();
    while (hs_iter != hashes.end() && nc_iter != nonces.end()) {
        syncPool_.Execute([n = *nc_iter, h = std::move(*hs_iter), peer, this]() {
            Bundle bundle(n);
            auto recs = GetMainChainLevelSet(h);
            if (recs.empty()) {
                SEND_MESSAGE(peer, NOT_FOUND, NotFound(h, n));
                return;
            }

            for (auto& b : recs) {
                bundle.AddBlock(b);
            }
            SEND_MESSAGE(peer, BUNDLE, bundle);
        });
        hs_iter++;
        nc_iter++;
    }
}

void DAGManager::BatchSync(std::vector<uint256>& requests, const PeerPtr& requestFrom) {
    std::vector<uint256> finalHashes;
    std::vector<GetDataTask> finalTasks;
    finalHashes.reserve(requests.size());
    finalTasks.reserve(requests.size());

    if (!preDownloading.Empty()) {
        // We will continue to send out the remaning hashes in preDownloading,
        // which are left last time this method is called, because the GetData
        // size exceeds the limit
        preDownloading.DrainTo(finalHashes, maxGetDataSize);
        for (const auto& h : finalHashes) {
            finalTasks.push_back(*RequestData(h, requestFrom));
            spdlog::debug("Sending another round of getDataTasks. preDownloading size = {}", preDownloading.Size());
        }
    } else {
        for (auto& h : requests) {
            auto task = RequestData(h, requestFrom);
            if (!task) {
                continue;
            }

            if (finalHashes.size() < maxGetDataSize) {
                finalHashes.push_back(h);
                finalTasks.push_back(*task);
            } else {
                preDownloading.Put(h);
            }
        }

        requests.clear();

        if (finalHashes.empty()) {
            // Force the downloading queue to check and remove existing hashes
            UpdateDownloadingQueue(Hash::GetZeroHash());
            return;
        }
    }

    // To avoid piling up too many level sets in memory
    while (storagePool_.GetTaskSize() > storagePool_.GetThreadSize() * 2) {
        std::this_thread::yield();
    }

    GetData message(GetDataTask::LEVEL_SET);

    auto hIter = finalHashes.begin();
    auto tIter = finalTasks.begin();
    while (hIter != finalHashes.end() && tIter != finalTasks.end()) {
        requestFrom->AddPendingGetDataTask(*tIter);
        message.AddItem(*hIter, tIter->id);
        downloading.insert(*hIter);
        ++hIter;
        ++tIter;
    }
    SEND_MESSAGE(requestFrom, GET_DATA, message);
}

std::vector<uint256> DAGManager::ConstructLocator(const uint256& fromHash, size_t length, const PeerPtr& peer) {
    std::vector<uint256> locator;
    const RecordPtr startMilestone = fromHash.IsNull() ? GetMilestoneHead() : GetState(fromHash);
    uint256 startMilestoneHash     = startMilestone->cblock->GetHash();

    // Did we already make this request? If so, abort.
    if (spdlog::default_logger()->level() <= spdlog::level::level_enum::debug) {
        if ((peer->GetLastGetInvBegin().IsNull() ? GENESIS.GetHash() : peer->GetLastGetInvBegin()) ==
                startMilestoneHash &&
            peer->GetLastGetInvLength() == length) {
            spdlog::debug("ConstructLocator({}, {}): duplicated request.", std::to_string(startMilestoneHash), length);
        }
        spdlog::debug("{}: downloadMilestones({}) current head = {}", peer->address.ToString(), length,
                      std::to_string(startMilestoneHash));
    }

    locator = TraverseMilestoneBackward(startMilestone, length);

    peer->SetLastGetInvBegin(locator.front());
    peer->SetLastGetInvLength(length);
    peer->SetLastGetInvEnd(locator.back());
    spdlog::debug("Constructed locator: \n   from   {}\n   to     {}\n   length {}", std::to_string(locator.front()),
                  std::to_string(locator.back()), locator.size());

    return locator;
}

std::vector<uint256> DAGManager::TraverseMilestoneBackward(RecordPtr cursor, size_t length) {
    std::vector<uint256> result;
    const auto& bestChain    = milestoneChains.best()->GetStates();
    size_t leastHeightCached = bestChain.front()->height;

    size_t cursorHeight = cursor->snapshot->height;
    assert(result.empty());

    // If the cursor height is within the cache range,
    // traverse the best chain cache.
    while (cursorHeight >= leastHeightCached && result.size() < length) {
        result.push_back(bestChain[cursorHeight - leastHeightCached]->GetMilestoneHash());
        if (cursorHeight == 0) {
            break;
        }
        --cursorHeight;
    }

    // If we have reached the least height in cache and still don't get enough hashes,
    // continue to traverse DB until we reach the capacity.
    while (result.size() < length && cursorHeight > 0) {
        result.push_back(CAT->GetMilestoneAt(cursorHeight)->cblock->GetHash());
        --cursorHeight;
    }

    return result;
}

std::vector<uint256> DAGManager::TraverseMilestoneForward(RecordPtr cursor, size_t length) {
    std::vector<uint256> result;
    const auto& bestChain    = milestoneChains.best()->GetStates();
    size_t leastHeightCached = bestChain.front()->height;

    size_t cursorHeight = cursor->snapshot->height;
    assert(result.empty());

    // If the cursor height is less than the least height in cache, traverse DB.
    while (cursorHeight < leastHeightCached && result.size() < length) {
        result.push_back(CAT->GetMilestoneAt(cursorHeight)->cblock->GetHash());
        ++cursorHeight;
    }

    // If we have reached the hightest milestone in DB and still don't get enough hashes,
    // continue to traverse the best chain cache until we reach the head or the capacity.
    while (result.size() < length && cursorHeight < leastHeightCached + bestChain.size()) {
        result.push_back(bestChain[cursorHeight - leastHeightCached]->GetMilestoneHash());
        ++cursorHeight;
    }

    return result;
}

bool DAGManager::UpdateDownloadingQueue(const uint256& hash) {
    auto size     = downloading.size();
    bool contains = downloading.erase(hash);
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

    if (downloading.empty()) {
        // GetInv for synchronization
        if (syncingPeer) {
            if (!preDownloading.Empty()) {
                std::vector<uint256> empty;
                BatchSync(empty, syncingPeer);
            } else {
                spdlog::info("Starting another run of synchronization. Current time - head time = {} s",
                             now - headTime);
                RequestInv(uint256(), 5, syncingPeer);
            }
        } else if (isBatchSynching.load()) {
            spdlog::error("EXIT: syncingPeer is null when isBatchSynching = {}", isBatchSynching);
            CompleteBatchSync();
        }
    }
    return contains;
}

void DAGManager::StartBatchSync(const PeerPtr& peer) {
    bool flag = false;
    if (isBatchSynching.compare_exchange_strong(flag, true)) {
        spdlog::info("We are behind our peers... start batch sync with peer {}.", peer->address.ToString());
        syncingPeer = peer;
    }
}

void DAGManager::CompleteBatchSync() {
    bool flag = true;
    if (isBatchSynching.compare_exchange_strong(flag, false)) {
        spdlog::info("Batch sync with {} completed.", syncingPeer->address.ToString());
        syncingPeer = nullptr;
    }
}

void DAGManager::DisconnectPeerSync(const PeerPtr& peer) {
    if (peer == syncingPeer) {
        CompleteBatchSync();
    }
}

/////////////////////////////////////
// End of synchronization methods
//

void DAGManager::AddNewBlock(ConstBlockPtr blk, PeerPtr peer) {
    verifyThread_.Execute([blk = std::move(blk), peer = std::move(peer), this] {
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
            RecordPtr ms = GetState(msHash);
            if (ms && !CheckPuntuality(blk, ms)) {
                return;
            }
            // Abort and send GetBlock requests.
            CAT->AddBlockToOBC(blk, mask());

            if (peer) {
                RequestInv(Hash::GetZeroHash(), 5, std::move(peer));
            }

            return;
        }

        // Check difficulty target //////

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

        // Check punctuality ////////////

        if (!CheckPuntuality(blk, ms)) {
            return;
        }

        // End of online verification
        /////////////////////////////////

        CAT->Cache(blk);

        if (peer) {
            peerManager->RelayBlock(blk, peer);
        }

        // TODO: erase transaction from mempool

        AddBlockToPending(blk);
        CAT->ReleaseBlocks(blk->GetHash());
    });
}

bool DAGManager::CheckPuntuality(const ConstBlockPtr& blk, const RecordPtr& ms) {
    assert(ms);

    if (blk->IsFirstRegistration()) {
        return true;
    }

    if (blk->GetMilestoneHash() == GENESIS.GetHash()) {
        return true;
    }

    if (blk->GetTime() - ms->cblock->GetTime() > GetParams().punctualityThred) {
        spdlog::info("Block is too old: {} vs. {} [{}]", blk->GetTime(), ms->cblock->GetTime(),
                     std::to_string(blk->GetHash()));
        return false;
    }

    if (GetBestMilestoneHeight() > ms->height &&
        (GetBestMilestoneHeight() - ms->height) > GetParams().cacheStatesSize) {
        spdlog::info("Block is too old: pointing to height {} vs. current head height {} [{}]", ms->height,
                     GetBestMilestoneHeight(), std::to_string(blk->GetHash()));
        return false;
    }
    return true;
}

void DAGManager::AddBlockToPending(const ConstBlockPtr& block) {
    spdlog::trace("AddBlockToPending {}", block->GetHash().to_substr());

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
            if (size_t level = FlushTrigger()) {
                FlushToCAT(level);
            }
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

    auto newMs  = chain->Verify(block);
    globalStates_.emplace(block->GetHash(), newMs);
    chain->AddNewState(*newMs);

    isVerifying = false;

    UpdateDownloadingQueue(block->GetHash());
}

RecordPtr DAGManager::GetState(const uint256& msHash) const {
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

const Chain& DAGManager::GetBestChain() const {
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
    forks.reserve(milestoneChains.size());
    for (auto& chain : milestoneChains) {
        if (chain == bestChain) {
            continue;
        }
        forks.emplace_back(chain->GetStates().begin());
    }

    auto cursor   = bestChain->GetStates().begin();
    size_t dupcnt = 0;
    for (bool flag = true; flag && dupcnt <= GetParams().cacheStatesToDelete; dupcnt++) {
        for (auto& fork_it : forks) {
            if (*cursor != *fork_it) {
                flag = false;
                break;
            }
            fork_it++;
        }
    }

    return dupcnt;
}

void DAGManager::FlushToCAT(size_t level) {
    // first store data to CAT
    auto [recToStore, utxoToStore, utxoToRemove] = milestoneChains.best()->GetDataToCAT(level);

    for (auto& lvsRec : recToStore) {
        std::swap(lvsRec.front(), lvsRec.back());
        CAT->StoreRecords(lvsRec);
    }
    for (const auto& [utxoKey, utxoPtr] : utxoToStore) {
        CAT->AddUTXO(utxoKey, utxoPtr);
    }
    for (const auto& utxoKey : utxoToRemove) {
        CAT->RemoveUTXO(utxoKey);
    }

    const auto oldestCs = milestoneChains.best()->GetStates().front();

    // then remove chain states from chains
    size_t totalRecNum = 0;
    std::for_each(recToStore.begin(), recToStore.end(), [&totalRecNum](const auto& lvs) { totalRecNum += lvs.size(); });
    std::vector<uint256> recHashes{};
    recHashes.reserve(totalRecNum);
    std::unordered_set<uint256> utxoCreated{};
    utxoCreated.reserve(utxoToStore.size());

    for (auto& lvsRec : recToStore) {
        for (auto& rec : lvsRec) {
            recHashes.emplace_back(rec->cblock->GetHash());
        }
    }

    for (const auto& [key, value] : utxoToStore) {
        utxoCreated.emplace(key);
    }
    TXOC txocToRemove{std::move(utxoCreated), std::move(utxoToRemove)};

    for (auto& chain : milestoneChains) {
        chain->PopOldest(recHashes, txocToRemove, level);
    }
}

bool CheckMsPOW(const ConstBlockPtr& b, const ChainStatePtr& m) {
    return !(UintToArith256(b->GetHash()) > m->milestoneTarget);
}

RecordPtr DAGManager::GetMilestoneHead() const {
    return GetState(GetBestChain().GetChainHead()->GetMilestoneHash());
}

size_t DAGManager::GetBestMilestoneHeight() const {
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
    auto rec = GetBestChain().GetRecord(blockHash);
    if (rec) {
        return rec->height;
    }
    return CAT->GetHeight(blockHash);
}

std::vector<ConstBlockPtr> DAGManager::GetMainChainLevelSet(size_t height) const {
    std::vector<ConstBlockPtr> lvs;
    const auto& bestChain    = GetBestChain();
    size_t leastHeightCached = bestChain.GetStates().front()->height;

    if (height < leastHeightCached) {
        for (auto& b : CAT->GetLevelSetAt(height)) {
            lvs.emplace_back(std::move(b));
        }
    } else {
        for (auto& h : bestChain.GetStates()[height - leastHeightCached]->GetRecordHashes()) {
            lvs.push_back(bestChain.GetRecord(h)->cblock);
        }
    }

    return lvs;
}

std::vector<ConstBlockPtr> DAGManager::GetMainChainLevelSet(const uint256& blockHash) const {
    return GetMainChainLevelSet(GetHeight(blockHash));
}

bool DAGManager::ExistsNode(const uint256& h) const {
    for (const auto& chain : milestoneChains) {
        if (chain->GetRecord(h)) {
            return true;
        }
    }
    return false;
}
