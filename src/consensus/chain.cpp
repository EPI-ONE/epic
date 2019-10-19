// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "chain.h"
#include "block_store.h"
#include "functors.h"
#include "mempool.h"
#include "tasm.h"

template <typename K, typename V>
bool UpdateKey(std::unordered_map<K, V>& m, const K& oldKey, const K& newKey) {
    auto entry = m.extract(oldKey);
    if (entry) {
        entry.key() = newKey;
        return m.insert(std::move(entry)).inserted;
    }

    return false;
}

template <typename K, typename V>
bool UpdateValue(std::unordered_map<K, V>& m, const K& key, const V& newValue) {
    auto entry = m.extract(key);
    if (entry) {
        entry.mapped() = newValue;
        return m.insert(std::move(entry)).inserted;
    }

    return false;
}

////////////////////
// Chain
////////////////////

Chain::Chain() : ismainchain_(true) {}

Chain::Chain(const Chain& chain, const ConstBlockPtr& pfork)
    : ismainchain_(false), states_(chain.states_), pendingBlocks_(chain.pendingBlocks_),
      recentHistory_(chain.recentHistory_), ledger_(chain.ledger_), prevRedempHashMap_(chain.prevRedempHashMap_) {
    if (states_.empty()) {
        return;
    }

    uint256 target = pfork->GetMilestoneHash();
    assert(recentHistory_.find(target) != recentHistory_.end());

    // We don't do any verification here but only data copying and rolling back
    for (auto it = states_.rbegin(); (*it)->GetMilestoneHash() != target && it != states_.rend(); it++) {
        for (const auto& rwp : (*it)->GetLevelSet()) {
            const auto& rpt = *rwp.lock();
            const auto& h   = rpt.cblock->GetHash();
            pendingBlocks_.insert({h, rpt.cblock});
            recentHistory_.erase(h);
            ledger_.Rollback((*it)->GetTXOC());

            // Rollback prevRedempHashMap_
            for (const auto& h : (*it)->GetRegChange().GetCreated()) {
                prevRedempHashMap_.erase(h.first);
            }
            for (const auto& h : (*it)->GetRegChange().GetRemoved()) {
                prevRedempHashMap_.insert(h);
            }
        }
        states_.erase(std::next(it).base());
    }
}

MilestonePtr Chain::GetChainHead() const {
    if (states_.empty()) {
        // Note: the lvs_ in the returned MilestonePtr contains a dummy weak_ptr that CANNOT be used
        return STORE->GetMilestoneAt(STORE->GetHeadHeight())->snapshot;
    }
    return states_.back();
}

void Chain::AddPendingBlock(ConstBlockPtr pblock) {
    pendingBlocks_.insert_or_assign(pblock->GetHash(), std::move(pblock));
}

void Chain::AddPendingUTXOs(std::vector<UTXOPtr>&& utxos) {
    if (utxos.empty()) {
        return;
    }
    for (auto& u : utxos) {
        ledger_.AddToPending(std::move(u));
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
    spdlog::debug("{} blocks sorted, {} pending blocks left. Total ratio: {}", result.size(), pendingBlocks_.size(),
                 static_cast<double>(result.size()) / (result.size() + pendingBlocks_.size()));
    return result;
}

void Chain::CheckTxPartition(Vertex& b, const arith_uint256& ms_hashrate) {
    if (b.minerChainHeight <= GetParams().sortitionThreshold) {
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

        ConstBlockPtr cursor = b.cblock;
        VertexPtr previous;
        while (!cum.Full()) {
            previous = GetVertex(cursor->GetPrevHash());

            if (!previous) {
                // cannot happen
                throw std::logic_error("Cannot find " + std::to_string(cursor->GetPrevHash()) + " in cumulatorMap.");
            }
            cum.Add(previous->cblock, false);
            cursor = previous->cblock;
        }

        cumulatorMap_.emplace(b.cblock->GetPrevHash(), cum);
    }

    auto nodeHandler = cumulatorMap_.extract(b.cblock->GetPrevHash());
    Cumulator& cum   = nodeHandler.mapped();

    // Allowed distance
    auto allowed = CalculateAllowedDist(cum, ms_hashrate);

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
    cum.Add(b.cblock, true);
    nodeHandler.key() = b.cblock->GetHash();
    cumulatorMap_.insert(std::move(nodeHandler));
}

VertexPtr Chain::Verify(const ConstBlockPtr& pblock) {
    auto height = GetChainHead()->height + 1;

    spdlog::debug("Verifying milestone block {} at height {}", pblock->GetHash().to_substr(), height);

    // get a path for validation by the post ordered DFS search
    std::vector<ConstBlockPtr> blocksToValidate = GetSortedSubgraph(pblock);

    std::vector<VertexPtr> vtcs;
    std::vector<VertexWPtr> wvtcs;
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
        if (vtx->cblock->IsFirstRegistration()) {
            const auto& blkHash = vtx->cblock->GetHash();
            prevRedempHashMap_.insert_or_assign(blkHash, blkHash);
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
        vtx->height = height;
        verifying_.insert({vtx->cblock->GetHash(), vtx});
    }

    CreateNextMilestone(GetChainHead(), *vtcs.back(), std::move(wvtcs), std::move(regChange), std::move(txoc));
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
    spdlog::trace("Validating {} at its miner chain {}", blkHash.to_substr(), vertex.minerChainHeight);

    // update the key of the prev redemption hashes
    uint256 oldRedempHash;
    if (UpdateKey(prevRedempHashMap_, prevHash, blkHash)) {
        oldRedempHash = prevRedempHashMap_.find(blkHash)->second;
    } else {
        oldRedempHash = STORE->GetPrevRedemHash(prevHash);
        prevRedempHashMap_.emplace(blkHash, oldRedempHash);
    }

    assert(!oldRedempHash.IsNull());
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
        VertexPtr prevMs = DAG->GetState(vertex.cblock->GetMilestoneHash());
        assert(prevMs);
        CheckTxPartition(vertex, prevMs->snapshot->hashRate);

        // check utxo
        // txns with valid utxo will have validity == VALID, and others are left unchanged
        validTXOC.Merge(ValidateTxns(vertex));

        // invalidate transactions that still have validity == UNKNOWN
        const auto& txns = vertex.cblock->GetTransactions();
        for (size_t i = 0; i < txns.size(); ++i) {
            if (!vertex.validity[i]) { // if UNKNOWN
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
    spdlog::trace("Validating redemption {}", blkHash.to_substr());

    uint256 prevRedempHash = GetPrevRedempHash(blkHash);
    auto prevReg           = GetVertex(prevRedempHash);
    assert(prevReg);

    if (prevReg->isRedeemed != Vertex::NOT_YET_REDEEMED) {
        spdlog::info("[Validation] Double redemption on previous registration block {} [{}]",
                     std::to_string(prevRedempHash), hashstr);
        return {};
    }

    const auto& redem = vertex.cblock->GetTransactions().at(0);
    const auto& vin   = redem->GetInputs().at(0);
    const auto& vout  = redem->GetOutputs().at(0); // only the first tx output will be regarded as valid

    auto prevBlock = GetVertex(vertex.cblock->GetPrevHash());
    // value of the output should be less or equal to the previous counter
    if (!(vout.value <= prevBlock->cumulativeReward)) {
        spdlog::info("[Validation] Wrong redemption value that exceeds total cumulative reward! [{}]", hashstr);
        return {};
    }

    if (!VerifyInOut(vin, prevReg->cblock->GetTransactions().at(0)->GetOutputs()[0].listingContent)) {
        spdlog::info("[Validation] Signature failed in redemption {} [{}]", vin.GetParentTx()->GetHash().to_substr(),
                     hashstr);
        return {};
    }

    // update redemption status
    prevReg->isRedeemed = Vertex::IS_REDEEMED;
    vertex.isRedeemed   = Vertex::NOT_YET_REDEEMED;
    regChange.Remove(blkHash, prevRedempHash);
    regChange.Create(blkHash, blkHash);
    UpdateValue(prevRedempHashMap_, blkHash, blkHash);

    return TXOC{{ComputeUTXOKey(blkHash, 0, 0)}, {}};
}

bool Chain::ValidateTx(const Transaction& tx, uint32_t index, TXOC& txoc, Coin& fee) {
    const auto& blkHash = tx.GetParentBlock()->GetHash();

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
    spdlog::trace("Validating transactions in block {}", blkHash.to_substr());

    TXOC validTXOC{};

    const auto& txns = vertex.cblock->GetTransactions();
    for (size_t i = 0; i < txns.size(); ++i) {
        if (vertex.validity[i]) { // if not UNKNWON
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

void Chain::PopOldest(const std::vector<uint256>& vtxToRemove, const TXOC& txocToRemove) {
    // remove vertices
    for (const auto& lvsh : vtxToRemove) {
        recentHistory_.erase(lvsh);
    }

    // remove utxos
    ledger_.Remove(txocToRemove);

    // remove states
    states_.pop_front();
}

std::tuple<std::vector<VertexWPtr>, std::unordered_map<uint256, UTXOPtr>, std::unordered_set<uint256>>
Chain::GetDataToSTORE(MilestonePtr ms) {
    std::vector<VertexWPtr> result_vtx = ms->GetLevelSet();
    TXOC result_txoc                   = ms->GetTXOC();

    std::unordered_map<uint256, UTXOPtr> result_created{};
    for (const auto& key_created : result_txoc.GetCreated()) {
        result_created.emplace(key_created, ledger_.FindFromLedger(key_created));
    }
    return {result_vtx, result_created, result_txoc.GetSpent()};
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

void Cumulator::Add(const ConstBlockPtr& block, bool ascending) {
    const auto& chainwork   = block->GetChainWork();
    uint32_t chainwork_comp = chainwork.GetCompact();

    if (timestamps.size() < GetParams().sortitionThreshold) {
        sum += chainwork;
    } else {
        arith_uint256 subtrahend = arith_uint256().SetCompact(chainworks.front().first);
        sum += (chainwork - subtrahend);

        // Pop the first element if the counter is already 1,
        // or decrease the counter of the first element by 1
        if (chainworks.front().second == 1) {
            chainworks.pop_front();
        } else {
            chainworks.front().second--;
        }

        timestamps.pop_front();
    }

    if (ascending) {
        if (!chainworks.empty() && chainworks.back().first == chainwork_comp) {
            chainworks.back().second++;
        } else {
            chainworks.emplace_back(chainwork_comp, 1);
        }
        timestamps.emplace_back(block->GetTime());
    } else {
        if (!chainworks.empty() && chainworks.front().first == chainwork_comp) {
            chainworks.front().second++;
        } else {
            chainworks.emplace_front(chainwork_comp, 1);
        }
        timestamps.emplace_front(block->GetTime());
    }
}

arith_uint256 Cumulator::Sum() const {
    return sum;
}

uint32_t Cumulator::TimeSpan() const {
    return timestamps.back() - timestamps.front();
}

bool Cumulator::Full() const {
    return timestamps.size() == GetParams().sortitionThreshold;
}

bool Cumulator::Empty() const {
    return timestamps.empty();
}

std::string std::to_string(const Cumulator& cum) {
    std::string s;
    s += " Cumulator { \n";
    s += "   chainworks { \n";
    for (auto& e : cum.chainworks) {
        s += strprintf("     { %s, %s }\n", arith_uint256().SetCompact(e.first).GetLow64(), e.second);
    }
    s += "   }\n";
    s += "   timestamps { \n";
    for (auto& t : cum.timestamps) {
        s += strprintf("     %s\n", t);
    }
    s += "   }\n";
    s += " }";

    return s;
}
