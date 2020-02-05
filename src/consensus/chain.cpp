// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "chain.h"
#include "block_store.h"
#include "functors.h"
#include "mempool.h"
#include "tasm.h"

////////////////////
// Chain
////////////////////

Chain::Chain() : ismainchain_(true) {}

Chain::Chain(const Chain& chain, const ConstBlockPtr& pfork)
    : ismainchain_(false), milestones_(chain.milestones_), pendingBlocks_(chain.pendingBlocks_),
      recentHistory_(chain.recentHistory_), ledger_(chain.ledger_), prevRedempHashMap_(chain.prevRedempHashMap_),
      prevRegsToModify_(chain.prevRegsToModify_) {
    if (milestones_.empty()) {
        return;
    }

    uint256 target = pfork->GetMilestoneHash();
    assert(recentHistory_.find(target) != recentHistory_.end());

    // We don't do any verification here but only data copying and rolling back
    for (auto it = milestones_.rbegin(); (*it)->GetMilestoneHash() != target && it != milestones_.rend(); it++) {
        for (const auto& rwp : (*it)->GetLevelSet()) {
            const auto& rpt = *rwp.lock();
            const auto& h   = rpt.cblock->GetHash();
            pendingBlocks_.insert({h, rpt.cblock});
            recentHistory_.erase(h);
            ledger_.Rollback((*it)->GetTXOC());

            // Rollback prevRedempHashMap_ and prevRedempBlockMap_
            for (const auto& entry : (*it)->GetRegChange().GetCreated()) {
                prevRedempHashMap_.erase(entry.first);
            }
            for (const auto& entry : (*it)->GetRegChange().GetRemoved()) {
                prevRedempHashMap_.insert(entry);
                prevRegsToModify_.erase(entry.second);
            }
        }
        milestones_.erase(std::next(it).base());
    }
}

MilestonePtr Chain::GetChainHead() const {
    if (milestones_.empty()) {
        // Note: the lvs_ in the returned MilestonePtr contains a dummy weak_ptr that CANNOT be used
        return STORE->GetMilestoneAt(STORE->GetHeadHeight())->snapshot;
    }
    return milestones_.back();
}

void Chain::AddPendingBlock(ConstBlockPtr pblock) {
    pendingBlocks_.insert_or_assign(pblock->GetHash(), std::move(pblock));
}

void Chain::AddPendingUTXOs(std::vector<UTXOPtr> utxos) {
    if (utxos.empty()) {
        return;
    }
    for (auto& u : utxos) {
        ledger_.AddToPending(u);
    }
}

bool Chain::IsBlockPending(const uint256& hash) const {
    return pendingBlocks_.find(hash) != pendingBlocks_.end();
}

std::vector<ConstBlockPtr> Chain::GetPendingBlocks() const {
    return pendingBlocks_.value_set();
}

std::vector<uint256> Chain::GetPendingHashes() const {
    return pendingBlocks_.key_set();
}

std::size_t Chain::GetPendingBlockCount() const {
    return pendingBlocks_.size();
}

ConstBlockPtr Chain::GetRandomTip() const {
    auto tip = pendingBlocks_.random_value();
    if (tip) {
        return *tip;
    }

    return nullptr;
}

std::vector<ConstBlockPtr> Chain::GetSortedSubgraph(const ConstBlockPtr& pblock) {
    std::vector<ConstBlockPtr> stack = {pblock};
    std::vector<ConstBlockPtr> result;
    ConstBlockPtr cursor;

    /**
     * reserve memory for efficiency;
     * please note that n/2 is a not really precise upper bound and can be improved
     */
    stack.reserve(pendingBlocks_.size() / 2);
    result.reserve(pendingBlocks_.size());

    while (!stack.empty()) {
        cursor = stack.back();

        auto swap = pendingBlocks_.find(cursor->GetMilestoneHash());
        if (swap != pendingBlocks_.end()) {
            stack.push_back(swap->second);
            continue;
        }

        swap = pendingBlocks_.find(cursor->GetPrevHash());
        if (swap != pendingBlocks_.end()) {
            stack.push_back(swap->second);
            continue;
        }

        swap = pendingBlocks_.find(cursor->GetTipHash());
        if (swap != pendingBlocks_.end()) {
            stack.push_back(swap->second);
            continue;
        }

        uint256 cursorHash = cursor->GetHash();
        pendingBlocks_.erase(cursorHash);
        result.push_back(cursor);
        stack.pop_back();
    }

    result.shrink_to_fit();
    spdlog::debug("[Validation] {} block(s) sorted, {} pending block(s) left. Ratio: {}", result.size(),
                  pendingBlocks_.size(), static_cast<double>(result.size()) / (result.size() + pendingBlocks_.size()));
    return result;
}

void Chain::CheckTxPartition(Vertex& b) {
    auto msLinkHeight = GetVertex(b.cblock->GetMilestoneHash())->height;
    if (msLinkHeight <= GetParams().sortitionThreshold) {
        if (b.cblock->IsRegistration()) {
            if (b.cblock->GetTransactionSize() > 1) {
                memset(&b.validity[1], Vertex::Validity::INVALID, b.validity.size() - 1);
                spdlog::info(
                    "[Validation] Does not reach height of partition threshold but contains transactions other than "
                    "registration [{}]",
                    std::to_string(b.cblock->GetHash()));
            }
        } else {
            memset(&b.validity[0], Vertex::Validity::INVALID, b.validity.size());
            spdlog::info(
                "[Validation] Does not reach height of partition threshold but contains non-reg transactions [{}]",
                std::to_string(b.cblock->GetHash()));
        }
        return;
    }

    auto search = cumulatorMap_.find(b.cblock->GetPrevHash());
    if (search == cumulatorMap_.end()) {
        // Construct a cumulator for the block if it is not cached
        Cumulator cum;

        Vertex blk_cursor = b;
        VertexPtr previous;
        while (previous->height > (previous->height - Cumulator::GetCap()) && previous->height > 0) {
            previous = GetVertex(blk_cursor.cblock->GetPrevHash());

            if (!previous) {
                // should not happen
                throw std::logic_error("Cannot find " + std::to_string(blk_cursor.cblock->GetPrevHash()) +
                                       " in cumulatorMap.");
            }
            cum.Add(*previous, *this, false);
            blk_cursor = *previous;
        }

        cumulatorMap_.emplace(b.cblock->GetPrevHash(), cum);
    }

    auto nodeHandler = cumulatorMap_.extract(b.cblock->GetPrevHash());
    Cumulator& cum   = nodeHandler.mapped();

    // Allowed distance
    auto allowed = CalculateAllowedDist(cum, msLinkHeight);

    // Distances of the transaction hashes and previous block hash
    const auto& txns = b.cblock->GetTransactions();
    for (size_t i = 0; i < txns.size(); ++i) {
        if (b.validity[i]) {
            continue;
        }

        auto dist = UintToArith256(txns[i]->GetHash()) ^ UintToArith256(b.cblock->GetPrevHash());

        if (!PartitionCmp(dist, allowed)) {
            b.validity[i] = Vertex::Validity::INVALID;
            spdlog::info("[Validation] Transaction distance exceeds its allowed distance! [{}]",
                         std::to_string(b.cblock->GetHash()));
        }
    }

    // Update key for the cumulator
    cum.Add(b, *this, true);
    nodeHandler.key() = b.cblock->GetHash();
    cumulatorMap_.insert(std::move(nodeHandler));
}

const Cumulator* Chain::GetCumulator(const uint256& h) const {
    auto it = cumulatorMap_.find(h);
    if (it == cumulatorMap_.end()) {
        return nullptr;
    }

    return &it->second;
}

VertexPtr Chain::Verify(const ConstBlockPtr& pblock) {
    auto height = GetChainHead()->height + 1;

    spdlog::debug("[Validation] Validating level set of ms {} at height {}", pblock->GetHash().to_substr(), height);

    // get a path for validation by the post ordered DFS search
    std::vector<ConstBlockPtr> blocksToValidate = GetSortedSubgraph(pblock);

    std::vector<VertexPtr> vtcs;
    std::vector<VertexWPtr> wvtcs;
    std::unordered_set<Cumulator*> cumulators;
    RegChange regChange;
    TXOC txoc;
    vtcs.reserve(blocksToValidate.size());
    wvtcs.reserve(blocksToValidate.size());
    verifying_.clear();

    for (const auto& b : blocksToValidate) {
        vtcs.emplace_back(std::make_shared<Vertex>(b));
        wvtcs.emplace_back(vtcs.back());
    }

    // validate each block in order
    for (auto& vtx : vtcs) {
        vtx->height = height;

        const auto& blkHash = vtx->cblock->GetHash();
        if (vtx->cblock->IsFirstRegistration()) {
            prevRedempHashMap_.insert_or_assign(blkHash, const_cast<uint256&&>(blkHash));
            vtx->isRedeemed = Vertex::NOT_YET_REDEEMED;
            regChange.Create(blkHash, blkHash);
            vtx->minerChainHeight = 1;
            vtx->validity[0]      = Vertex::Validity::VALID;
            // Invalidate any txns other than the first registration in this block
            memset(&vtx->validity[1], Vertex::Validity::INVALID, vtx->validity.size() - 1);
        } else {
            auto [validTXOC, invalidTXOC] = Validate(*vtx, regChange);

            // update ledger in chain for future reference
            if (!validTXOC.Empty()) {
                ledger_.Update(validTXOC);
                txoc.Merge(std::move(validTXOC));
            }

            if (!invalidTXOC.Empty()) {
                // move utxos of the block from pending to removed
                ledger_.Invalidate(invalidTXOC);
                txoc.Merge(std::move(invalidTXOC));
            }

            for (const auto& v : vtx->validity) {
                assert(v);
            }

            vtx->UpdateReward(GetPrevReward(*vtx));
        }
        verifying_.insert({vtx->cblock->GetHash(), vtx});

        auto cum = cumulatorMap_.find(blkHash);
        if (cum != cumulatorMap_.end()) {
            cumulators.insert(&(cum->second));
        }
    }

    CreateNextMilestone(GetChainHead(), *vtcs.back(), std::move(wvtcs), std::move(regChange), std::move(txoc));
    const auto& ms = vtcs.back()->snapshot;
    spdlog::debug("[Validation] New milestone {} has milestone difficulty target in compact form {} as difficulty {}",
                  vtcs.back()->cblock->GetHash().to_substr(), ms->milestoneTarget.GetCompact(), ms->GetMsDifficulty());
    vtcs.back()->UpdateMilestoneReward();

    recentHistory_.merge(std::move(verifying_));
    return vtcs.back();
}

std::pair<TXOC, TXOC> Chain::Validate(Vertex& vertex, RegChange& regChange) {
    const auto& pblock   = vertex.cblock;
    const auto& blkHash  = pblock->GetHash();
    const auto& prevHash = pblock->GetPrevHash();

    // update miner chain height
    vertex.minerChainHeight = GetVertex(prevHash)->minerChainHeight + 1;
    spdlog::trace("[Validation] Validating {} at its miner chain {}", blkHash.to_substr(), vertex.minerChainHeight);

    // update the key of the prev redemption hashes
    uint256 oldRedempHash;
    if (prevRedempHashMap_.update_key(prevHash, blkHash)) {
        oldRedempHash = prevRedempHashMap_.find(blkHash)->second;
    } else {
        oldRedempHash = STORE->GetPrevRedemHash(prevHash);

        if (oldRedempHash.IsNull()) {
            spdlog::warn("[Validation] Peer chain forks here [{}]", std::to_string(blkHash));
            auto b = GetVertex(prevHash);
            while (!b->cblock->IsRegistration() || b->validity[0] != Vertex::VALID) {
                b = GetVertex(b->cblock->GetPrevHash());
            }
            oldRedempHash = b->cblock->GetHash();
        }

        prevRedempHashMap_.emplace(blkHash, oldRedempHash);
    }

    regChange.Remove(prevHash, oldRedempHash);
    regChange.Create(blkHash, oldRedempHash);

    // verify its transaction and return the updating UTXO
    TXOC validTXOC, invalidTXOC;
    if (pblock->HasTransaction()) {
        if (pblock->IsRegistration()) {
            auto result = ValidateRedemption(vertex, regChange);
            if (result) {
                vertex.validity[0] = Vertex::Validity::VALID;
                validTXOC.Merge(*result);
            } else {
                vertex.validity[0] = Vertex::Validity::INVALID;
                invalidTXOC.Merge(CreateTXOCFromInvalid(*vertex.cblock->GetTransactions()[0], 0));
            }
        } // by now, registrations (validity[0]) must != UNKNOWN (either VALID or INVALID)

        // check partition
        // txns with invalid distance will have validity == INVALID, and others are left unchanged
        VertexPtr prevMs = DAG->GetMsVertex(vertex.cblock->GetMilestoneHash());
        assert(prevMs);
        CheckTxPartition(vertex);

        // check utxo
        // txns with valid utxo will have validity == VALID, and others are left unchanged
        validTXOC.Merge(ValidateTxns(vertex));

        // invalidate transactions that still have validity == UNKNOWN
        const auto& txns = vertex.cblock->GetTransactions();
        for (size_t i = 0; i < txns.size(); ++i) {
            if (vertex.validity[i] == Vertex::Validity::UNKNOWN) {
                vertex.validity[i] = Vertex::Validity::INVALID;
                invalidTXOC.Merge(CreateTXOCFromInvalid(*txns[i], i));
            }

            if (MEMPOOL) {
                MEMPOOL->ReleaseTxFromConfirmed(txns[i], vertex.validity[i] == Vertex::Validity::VALID);
            }
        }
    }

    return std::make_pair(validTXOC, invalidTXOC);
}

uint256 Chain::GetPrevRedempHash(const uint256& h) const {
    auto entry = prevRedempHashMap_.find(h);
    if (entry != prevRedempHashMap_.end()) {
        return entry->second;
    }
    return STORE->GetPrevRedemHash(h);
}

std::optional<TXOC> Chain::ValidateRedemption(Vertex& vertex, RegChange& regChange) {
    const auto& blkHash = vertex.cblock->GetHash();
    const auto hashstr  = std::to_string(blkHash);
    spdlog::trace("[Validation] Validating redemption in block {}", blkHash.to_substr());

    uint256 prevRedempHash = GetPrevRedempHash(blkHash);
    VertexPtr prevReg      = GetVertex(prevRedempHash);
    assert(prevReg);

    const auto& redem = vertex.cblock->GetTransactions().at(0);
    const auto& vin   = redem->GetInputs().at(0);
    const auto& vout  = redem->GetOutputs().at(0); // only the first tx output will be regarded as valid

    if (vin.outpoint.bHash != prevRedempHash) {
        spdlog::info("[Validation] Invalid redemption on the previous registration block: outpoint {} not matching the "
                     "last valid redemption hash {} [{}]",
                     vin.outpoint.bHash.to_substr(), prevRedempHash.to_substr(), hashstr);
        return {};
    }

    if (prevReg->isRedeemed != Vertex::NOT_YET_REDEEMED || prevRegsToModify_.contains(prevRedempHash)) {
        spdlog::info("[Validation] Double redemption on the previous registration block: already redeemed {} [{}]",
                     prevRedempHash.to_substr(), hashstr);
        return {};
    }

    auto prevBlock = GetVertex(vertex.cblock->GetPrevHash());
    // value of the output should be less or equal to the previous counter
    if (!(vout.value <= prevBlock->cumulativeReward)) {
        spdlog::info("[Validation] Wrong redemption value ({}) that exceeds the total cumulative reward ({}) [{}]",
                     vout.value.GetValue(), prevBlock->cumulativeReward.GetValue(), hashstr);
        return {};
    }

    if (!VerifyInOut(vin, prevReg->cblock->GetTransactions().at(0)->GetOutputs()[0].listingContent)) {
        spdlog::info("[Validation] Signature failed in redemption {} [{}]", vin.GetParentTx()->GetHash().to_substr(),
                     hashstr);
        return {};
    }

    // update redemption status
    prevRegsToModify_.emplace(prevRedempHash);
    vertex.isRedeemed = Vertex::NOT_YET_REDEEMED;
    regChange.Remove(blkHash, prevRedempHash);
    regChange.Create(blkHash, blkHash);
    prevRedempHashMap_.update_value(blkHash, blkHash);

    return TXOC{{ComputeUTXOKey(blkHash, 0, 0)}, {}};
}

bool Chain::ValidateTx(const Transaction& tx, uint32_t index, TXOC& txoc, Coin& fee) {
    const auto& blkHash = tx.GetParentBlock()->GetHash();
    spdlog::trace("[Validation] Validating tx {} in block {}", tx.GetHash().to_substr(), blkHash.to_substr());

    Coin valueIn{};
    Coin valueOut{};
    std::vector<Tasm::Listing> prevOutListing{};
    prevOutListing.reserve(tx.GetInputs().size());

    // check previous vouts that are used in this transaction and compute total value in along the way
    for (const auto& vin : tx.GetInputs()) {
        const TxOutPoint& outpoint = vin.outpoint;
        // this ensures that $prevOut has not been spent yet
        auto prevOut = ledger_.FindSpendable(ComputeUTXOKey(outpoint.bHash, outpoint.txIndex, outpoint.outIndex));

        if (!prevOut) {
            spdlog::info("[Validation] Attempting to spend a non-existent or spent output {} in tx {} [{}]",
                         std::to_string(outpoint), tx.GetHash().to_substr(), std::to_string(blkHash));
            return false;
        }
        valueIn += prevOut->GetOutput().value;

        prevOutListing.emplace_back(prevOut->GetOutput().listingContent);
        txoc.AddToSpent(vin);
    }

    // get key of new UTXO and compute total value out
    for (size_t j = 0; j < tx.GetOutputs().size(); ++j) {
        const auto& out = tx.GetOutputs()[j];
        if (out.value > valueIn) {
            // To prevent overflow of the outputs' sum
            spdlog::info("[Validation] Transaction {} has an output whose value ({}) is greater than the sum of all "
                         "inputs ({}) [{}]",
                         tx.GetHash().to_substr(), out.value.GetValue(), valueIn.GetValue(), std::to_string(blkHash));
            return false;
        }
        valueOut += out.value;
        txoc.AddToCreated(blkHash, index, j);
    }

    // check total amount of value in and value out and record the fee received
    fee = valueIn - valueOut;
    if (!(valueIn >= valueOut && fee <= GetParams().maxMoney)) {
        spdlog::info("[Validation] Transaction {} input value goes out of range! [{}]", tx.GetHash().to_substr(),
                     std::to_string(blkHash));
        return false;
    }

    // verify transaction input one by one
    auto itprevOut = prevOutListing.cbegin();
    for (const auto& input : tx.GetInputs()) {
        if (!VerifyInOut(input, *itprevOut)) {
            spdlog::info("[Validation] Signature failed in tx {}! [{}]", tx.GetHash().to_substr(),
                         std::to_string(blkHash));
            return false;
        }
        itprevOut++;
    }

    return true;
}

TXOC Chain::ValidateTxns(Vertex& vertex) {
    const auto& blkHash = vertex.cblock->GetHash();
    spdlog::trace("[Validation] Validating transactions in block {}", blkHash.to_substr());

    TXOC validTXOC{};

    const auto& txns = vertex.cblock->GetTransactions();
    for (size_t i = 0; i < txns.size(); ++i) {
        if (vertex.validity[i] != Vertex::Validity::UNKNOWN) {
            // Skipping, because this txn is either redemption
            // or has been marked invalid by CheckTxPartition
            continue;
        }

        TXOC txoc{};
        Coin fee{};
        if (ValidateTx(*txns[i], i, txoc, fee)) {
            vertex.fee += fee;
            validTXOC.Merge(std::move(txoc));
            vertex.validity[i] = Vertex::Validity::VALID;
        }
    }

    return validTXOC;
}

VertexPtr Chain::GetVertexCache(const uint256& blkHash) const {
    auto result = verifying_.find(blkHash);
    if (result == verifying_.end()) {
        result = recentHistory_.find(blkHash);
        if (result == recentHistory_.end()) {
            return nullptr;
        }
    }

    return result->second;
}

VertexPtr Chain::GetVertex(const uint256& blkHash) const {
    auto result = GetVertexCache(blkHash);
    if (!result) {
        result = STORE->GetVertex(blkHash);
    }

    return result;
}

VertexPtr Chain::GetMsVertexCache(const uint256& msHash) const {
    auto entry = recentHistory_.find(msHash);
    if (entry != recentHistory_.end() && entry->second->isMilestone) {
        return entry->second;
    }
    return nullptr;
}

MilestonePtr Chain::GetMsVertex(size_t height) const {
    size_t leastHeightCached = GetLeastHeightCached();

    if (height < leastHeightCached) {
        return STORE->GetMilestoneAt(height)->snapshot;
    } else {
        return milestones_[height - leastHeightCached];
    }
}

void Chain::PopOldest(const std::vector<uint256>& vtxToRemove, const TXOC& txocToRemove) {
    for (const auto& lvsh : vtxToRemove) {
        // Modify redemption status for those prev regs in DB
        const auto& vtx = GetVertexCache(lvsh);
        assert(vtx);
        if (!vtx->cblock->IsFirstRegistration() && vtx->cblock->IsRegistration() && vtx->validity[0] == Vertex::VALID) {
            auto h = vtx->cblock->GetTransactions()[0]->GetInputs()[0].outpoint.bHash;
            assert(prevRegsToModify_.contains(h));

            STORE->UpdateRedemptionStatus(h);
            prevRegsToModify_.erase(h);
        }

        recentHistory_.erase(lvsh);
    }

    // remove utxos
    ledger_.Remove(txocToRemove);

    // remove milestone
    milestones_.pop_front();
}

std::tuple<std::vector<VertexWPtr>, std::unordered_map<uint256, UTXOPtr>, std::unordered_set<uint256>>
Chain::GetDataToSTORE(MilestonePtr ms) {
    std::vector<VertexWPtr> result_vtx = ms->GetLevelSet();
    TXOC result_txoc                   = ms->GetTXOC();

    std::unordered_map<uint256, UTXOPtr> result_created{};
    for (const auto& key_created : result_txoc.GetCreated()) {
        result_created.emplace(key_created, ledger_.FindFromLedger(key_created));
    }
    return {std::move(result_vtx), std::move(result_created), result_txoc.GetSpent()};
}

std::vector<uint256> Chain::GetPeerChainHead() const {
    std::vector<uint256> result;
    result.reserve(prevRedempHashMap_.size());

    const auto keySet = prevRedempHashMap_.key_set();
    for (const auto& elem : keySet) {
        result.emplace_back(elem);
    }
    return result;
}

bool Chain::IsMilestone(const uint256& blkHash) const {
    auto result = recentHistory_.find(blkHash);
    if (result == recentHistory_.end()) {
        return STORE->IsMilestone(blkHash);
    }
    return result->second->isMilestone;
}

bool Chain::IsTxFitsLedger(const ConstTxPtr& tx) const {
    // check each input
    for (const auto& input : tx->GetInputs()) {
        if (!ledger_.IsSpendable(input.outpoint.GetOutKey())) {
            return false;
        }
    }
    return true;
}

////////////////////
// Cumulator
////////////////////

void Cumulator::Add(const Vertex& block, const Chain& chain, bool ascending) {
    auto msHeight = block.height;
    assert(msHeight > 0);

    // Update queue
    if (ascending) {
        // Align the segmt_sizes_
        auto back_height = sizes_.back().first;
        while (back_height++ < msHeight) {
            // This happens when there is level set contains
            // no block from this miner chain.

            if (Full()) {
                sizes_.pop_front();
            }

            sizes_.emplace_back(back_height, std::make_pair(chain.GetMsVertex(back_height)->lvsSize, 0));
        }

        sizes_.back().second.second++;

    } else {
        auto front_height = sizes_.front().first;
        while (front_height-- > msHeight) {
            sizes_.emplace_front(front_height, std::make_pair(chain.GetMsVertex(front_height)->lvsSize, 0));

            // We don't check whether the queue is full here.
            // It's the caller's responsibility to make sure that
            // the capacity is not exceeded when adding elements
            // backwards.
        }

        sizes_.front().second.second++;
    }
}

double Cumulator::Percentage(size_t height) const {
    if (sum_cache_.size() > GetParams().punctualityThred) {
        sum_cache_.erase(sizes_.back().first - GetParams().punctualityThred);
    }

    auto sums_it = sum_cache_.find(height);
    if (sums_it == sum_cache_.end()) {
        auto cursor = sizes_.rbegin();
        while (cursor->first != height) {
            cursor++;
        }

        auto sums = cursor->second;
        cursor++;
        while (cursor->first > (height - GetParams().sortitionThreshold)) {
            sums.first += cursor->second.first;
            sums.second += cursor->second.second;

            cursor++;
        }

        sum_cache_.emplace(height, sums);

        return sums.second / sums.first;
    }

    return sums_it->second.first / sums_it->second.second;
}

bool Cumulator::Full() const {
    return sizes_.size() == cap_;
}

bool Cumulator::Empty() const {
    return sizes_.empty();
}

void Cumulator::Clear() {
    sizes_.clear();
}

std::string std::to_string(const Cumulator& cum) {
    std::string s;
    s += " Cumulator { \n";
    s += "   sizes { \n";
    for (auto& e : cum.sizes_) {
        s += strprintf("     { %s, %s, %s}\n", e.first, e.second.first, e.second.second);
    }
    s += "   }\n";
    s += " }";

    return s;
}
