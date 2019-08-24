#include "chain.h"
#include "caterpillar.h"
#include "mempool.h"
#include "tasm/functors.h"
#include "tasm/tasm.h"

////////////////////
// Chain
////////////////////

Chain::Chain() : ismainchain_(true) {}

Chain::Chain(const Chain& chain, const ConstBlockPtr& pfork)
    : ismainchain_(false), states_(chain.states_), pendingBlocks_(chain.pendingBlocks_),
      recordHistory_(chain.recordHistory_), ledger_(chain.ledger_) {
    if (states_.empty()) {
        return;
    }

    uint256 target = pfork->GetMilestoneHash();
    assert(recordHistory_.find(target) != recordHistory_.end());

    for (auto it = states_.rbegin(); (*it)->GetMilestoneHash() != target && it != states_.rend(); it++) {
        for (const auto& rwp : (*it)->GetLevelSet()) {
            const auto& rpt = *rwp.lock();
            const auto& h   = rpt.cblock->GetHash();
            pendingBlocks_.insert({h, rpt.cblock});
            recordHistory_.erase(h);
            ledger_.Rollback((*it)->GetTXOC());
        }
        states_.erase(std::next(it).base());
    }
    // note that we don't do any verification here
}

ChainStatePtr Chain::GetChainHead() const {
    if (states_.empty()) {
        // Note: the lvs_ in the returned ChainStatePtr contains a dummy weak_ptr that CANNOT be used
        return CAT->GetMilestoneAt(CAT->GetHeadHeight())->snapshot;
    }
    return states_.back();
}

void Chain::AddPendingBlock(ConstBlockPtr pblock) {
    pendingBlocks_.insert_or_assign(pblock->GetHash(), std::move(pblock));
}

void Chain::AddPendingUTXOs(const std::vector<UTXOPtr>& utxos) {
    if (utxos.empty()) {
        return;
    }
    for (const auto& u : utxos) {
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

    /* reserve a good chunk of memory for efficiency;
     * please note that n/2 is a not really precise
     * upper bound and can be improved */
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
    return result;
}

void Chain::CheckTxPartition(NodeRecord& b, const arith_uint256& ms_hashrate) {
    if (b.minerChainHeight <= GetParams().sortitionThreshold) {
        if (b.cblock->IsRegistration()) {
            if (b.cblock->GetTransactionSize() > 1) {
                memset(&b.validity[1], NodeRecord::Validity::INVALID, b.validity.size() - 1);
                spdlog::info("Does not reach height of partition threshold but contains transactions other than "
                             "registration [{}]",
                             std::to_string(b.cblock->GetHash()));
            }
        } else {
            memset(&b.validity[0], NodeRecord::Validity::INVALID, b.validity.size());
            spdlog::info("Does not reach height of partition threshold but contains non-reg transactions [{}]",
                         std::to_string(b.cblock->GetHash()));
        }
        return;
    }

    auto search = cumulatorMap_.find(b.cblock->GetPrevHash());
    if (search == cumulatorMap_.end()) {
        // Construct a cumulator for the block if it is not cached
        Cumulator cum;

        ConstBlockPtr cursor = b.cblock;
        RecordPtr previous;
        while (!cum.Full()) {
            previous = GetRecord(cursor->GetPrevHash());

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
    auto allowed = (cum.Sum() / (cum.TimeSpan() + 1)) / GetParams().sortitionCoefficient *
                   (GetParams().maxTarget / (ms_hashrate + 1));

    // Distances of the transaction hashes and previous block hash
    const auto& txns = b.cblock->GetTransactions();
    for (size_t i = 0; i < txns.size(); ++i) {
        if (b.validity[i]) {
            continue;
        }

        auto dist = UintToArith256(txns[i]->GetHash()) ^ UintToArith256(b.cblock->GetPrevHash());

        if (!PartitionCmp(dist, allowed)) {
            b.validity[i] = NodeRecord::Validity::INVALID;
            spdlog::info("Transaction distance exceeds its allowed distance! [{}]",
                         std::to_string(b.cblock->GetHash()));
        }
    }

    // Update key for the cumulator
    cum.Add(b.cblock, true);
    nodeHandler.key() = b.cblock->GetHash();
    cumulatorMap_.insert(std::move(nodeHandler));
}

RecordPtr Chain::Verify(const ConstBlockPtr& pblock) {
    spdlog::debug("Verifying milestone block {} on height {}", pblock->GetHash().to_substr(), GetChainHead()->height);

    // get a path for validation by the post ordered DFS search
    std::vector<ConstBlockPtr> blocksToValidate = GetSortedSubgraph(pblock);

    std::vector<RecordPtr> recs;
    std::vector<RecordWPtr> wrecs;
    recs.reserve(blocksToValidate.size());
    wrecs.reserve(blocksToValidate.size());
    verifying_.clear();

    for (const auto& b : blocksToValidate) {
        recs.emplace_back(std::make_shared<NodeRecord>(b));
        wrecs.emplace_back(recs.back());
    }
    auto state = CreateNextChainState(GetChainHead(), *recs.back(), std::move(wrecs));

    // validate each block in order
    for (auto& rec : recs) {
        if (rec->cblock->IsFirstRegistration()) {
            const auto& blkHash = rec->cblock->GetHash();
            rec->prevRedemHash  = blkHash;
            rec->isRedeemed     = NodeRecord::NOT_YET_REDEEMED;
            state->regChange.Create(blkHash, blkHash);
            rec->minerChainHeight = 1;
        } else {
            TXOC validTXOC, invalidTXOC;
            Validate(*rec, state->regChange, validTXOC, invalidTXOC);

            // update ledger in chain for future reference
            if (!validTXOC.Empty()) {
                ledger_.Update(validTXOC);
                // take notes in chain state; will be used when flushing this state from memory to CAT
                state->UpdateTXOC(std::move(validTXOC));
            }

            if (!invalidTXOC.Empty()) {
                // remove utxo of the block from pending to removed
                ledger_.Invalidate(invalidTXOC);
                // still take notes in chain state
                state->UpdateTXOC(std::move(invalidTXOC));
            }

            for (const auto& v : rec->validity) {
                assert(v);
            }

            rec->UpdateReward(GetPrevReward(*rec));
        }
        rec->height = state->height;
        verifying_.insert({rec->cblock->GetHash(), rec});
    }

    recordHistory_.merge(std::move(verifying_));
    return recs.back();
}

void Chain::Validate(NodeRecord& record, RegChange& regChange, TXOC& validTXOC, TXOC& invalidTXOC) {
    spdlog::trace("Validating {}", record.cblock->GetHash().to_substr());
    const auto& pblock = record.cblock;
    std::optional<TXOC> result;

    // first update the prev redemption hashes
    auto prevRec = GetRecord(pblock->GetPrevHash());

    const auto& oldRedemHash = prevRec->prevRedemHash;
    assert(!oldRedemHash.IsNull());
    record.prevRedemHash = oldRedemHash;
    regChange.Remove(record.cblock->GetPrevHash(), oldRedemHash);
    regChange.Create(record.cblock->GetHash(), oldRedemHash);

    record.minerChainHeight = prevRec->minerChainHeight + 1;

    // then verify its transaction and return the updating UTXO
    if (pblock->HasTransaction()) {
        if (pblock->IsRegistration()) {
            result = ValidateRedemption(record, regChange);
            if (result) {
                record.validity[0] = NodeRecord::Validity::VALID;
                validTXOC.Merge(*result);
            } else {
                record.validity[0] = NodeRecord::Validity::INVALID;
                invalidTXOC.Merge(CreateTXOCFromInvalid(*record.cblock->GetTransactions()[0], 0));
            }
        } // by now, registrations (validity[0]) must != UNKNOWN (either VALID or INVALID)

        // check partition
        // txns with invalid distance will have validity == INVALID, and others are left unchanged
        RecordPtr prevMs = DAG->GetState(record.cblock->GetMilestoneHash());
        assert(prevMs);
        CheckTxPartition(record, prevMs->snapshot->hashRate);

        // check utxo
        // txns with valid utxo will have validity == VALID, and others are left unchanged
        auto valid = ValidateTxns(record);
        validTXOC.Merge(std::move(valid));

        // invalidate transactions that still have validity == UNKNOWN
        const auto& txns = record.cblock->GetTransactions();
        for (size_t i = 0; i < txns.size(); ++i) {
            if (!record.validity[i]) { // if UNKNOWN
                record.validity[i] = NodeRecord::Validity::INVALID;
                invalidTXOC.Merge(CreateTXOCFromInvalid(*txns[i], i));
            }

            MEMPOOL->ReleaseTxFromConfirmed(txns[i], record.validity[i] == NodeRecord::Validity::VALID);
        }
    }
}

std::optional<TXOC> Chain::ValidateRedemption(NodeRecord& record, RegChange& regChange) {
    spdlog::trace("Validating redemption {}", record.cblock->GetHash().to_substr());
    // this transaction is a redemption of reward
    auto prevReg = GetRecord(record.prevRedemHash);
    assert(prevReg);

    const auto& hashstr = std::to_string(record.cblock->GetHash());

    if (prevReg->isRedeemed != NodeRecord::NOT_YET_REDEEMED) {
        spdlog::info("Double redemption on previous registration block {} [{}]", std::to_string(record.prevRedemHash),
                     hashstr);
        return {};
    }

    const auto& redem = record.cblock->GetTransactions().at(0);
    const auto& vin   = redem->GetInputs().at(0);
    const auto& vout  = redem->GetOutputs().at(0); // only first tx output will be regarded as valid

    auto prevBlock = GetRecord(record.cblock->GetPrevHash());
    // value of the output should be less or equal to the previous counter
    if (!(vout.value <= prevBlock->cumulativeReward)) {
        spdlog::info("Wrong redemption value that exceeds total cumulative reward! [{}]", hashstr);
        return {};
    }

    if (!VerifyInOut(vin, prevReg->cblock->GetTransactions().at(0)->GetOutputs()[0].listingContent)) {
        spdlog::info("Singature failed! [{}]", hashstr);
        return {};
    }

    // update redemption status
    prevReg->isRedeemed  = NodeRecord::IS_REDEEMED;
    record.isRedeemed    = NodeRecord::NOT_YET_REDEEMED;
    const auto& oldRH    = record.prevRedemHash;
    const auto& blkHash  = record.cblock->GetHash();
    record.prevRedemHash = blkHash;
    regChange.Remove(blkHash, oldRH);
    regChange.Create(blkHash, blkHash);

    return TXOC{{ComputeUTXOKey(record.cblock->GetHash(), 0, 0)}, {}};
}

TXOC Chain::ValidateTxns(NodeRecord& record) {
    const auto& blkHash = record.cblock->GetHash();
    spdlog::trace("Validating transactions in block {}", blkHash.to_substr());

    TXOC validTXOC{};

    const auto& txns = record.cblock->GetTransactions();
    for (size_t i = 0; i < txns.size(); ++i) {
        if (record.validity[i]) { // if not UNKNWON
            // Skipping, because this txn is either redemption
            // or has been marked invalid by CheckTxPartition
            continue;
        }

        const auto& tx = txns[i];
        // update TXOC
        Coin valueIn{};
        Coin valueOut{};
        TXOC txoc{};
        std::vector<Tasm::Listing> prevOutListing{};
        prevOutListing.reserve(tx->GetInputs().size());

        // check previous vouts that are used in this transaction and compute total value in along the way
        for (const auto& vin : tx->GetInputs()) {
            const TxOutPoint& outpoint = vin.outpoint;
            // this ensures that $prevOut has not been spent yet
            auto prevOut = ledger_.FindSpendable(ComputeUTXOKey(outpoint.bHash, outpoint.txIndex, outpoint.outIndex));

            if (!prevOut) {
                spdlog::info("Attempting to spend a non-existent or spent output {} in tx {} [{}]",
                             std::to_string(outpoint), std::to_string(tx->GetHash()), std::to_string(blkHash));
                continue;
            }
            valueIn += prevOut->GetOutput().value;

            prevOutListing.emplace_back(prevOut->GetOutput().listingContent);
            txoc.AddToSpent(vin);
        }

        // get key of new UTXO and compute total value out
        for (size_t j = 0; j < tx->GetOutputs().size(); ++j) {
            valueOut += tx->GetOutputs()[j].value;
            txoc.AddToCreated(blkHash, i, j);
        }

        // check total amount of value in and value out and take a note of fee received
        Coin fee = valueIn - valueOut;
        if (!(fee >= 0 && fee <= GetParams().maxMoney)) {
            spdlog::info("Transaction {} input value goes out of range! [{}]", std::to_string(tx->GetHash()),
                         std::to_string(blkHash));
            continue;
        }

        // verify transaction input one by one
        auto itprevOut = prevOutListing.cbegin();
        for (const auto& input : tx->GetInputs()) {
            if (!VerifyInOut(input, *itprevOut)) {
                spdlog::info("Singature failed in tx {}! [{}]", std::to_string(tx->GetHash()), std::to_string(blkHash));
                continue;
            }
            itprevOut++;
        }

        record.fee += fee;
        validTXOC.Merge(std::move(txoc));
        record.validity[i] = NodeRecord::Validity::VALID;
    }

    return validTXOC;
}

RecordPtr Chain::GetRecordCache(const uint256& blkHash) const {
    auto result = verifying_.find(blkHash);
    if (result == verifying_.end()) {
        result = recordHistory_.find(blkHash);
        if (result == recordHistory_.end()) {
            return nullptr;
        }
    }

    return result->second;
}

RecordPtr Chain::GetRecord(const uint256& blkHash) const {
    auto result = GetRecordCache(blkHash);
    if (!result) {
        result = CAT->GetRecord(blkHash);
        if (result) {
            result->prevRedemHash = CAT->GetPrevRedemHash(blkHash);
        }
    }

    return result;
}

RecordPtr Chain::GetMsRecordCache(const uint256& msHash) const {
    auto entry = recordHistory_.find(msHash);
    if (entry != recordHistory_.end() && entry->second->isMilestone) {
        return entry->second;
    }
    return nullptr;
}

void Chain::PopOldest(const std::vector<uint256>& recToRemove, const TXOC& txocToRemove, size_t level) {
    // remove records
    for (const auto& lvsh : recToRemove) {
        recordHistory_.erase(lvsh);
    }

    // remove utxos
    ledger_.Remove(txocToRemove);

    // remove states
    for (size_t i = 0; i < level; i++) {
        states_.pop_front();
    }
}

std::tuple<std::vector<std::vector<RecordWPtr>>, std::unordered_map<uint256, UTXOPtr>, std::unordered_set<uint256>>
Chain::GetDataToCAT(size_t level) {
    std::vector<std::vector<RecordWPtr>> result_rec{};
    result_rec.reserve(level);
    TXOC result_txoc{};

    // traverse from the oldest chain state in memory
    for (auto cs_it = states_.begin(); cs_it < states_.begin() + level && cs_it != states_.end(); cs_it++) {
        result_rec.emplace_back((*cs_it)->GetLevelSet());

        // get all of the TXOC and merge them in advance to save space
        auto txoc = (*cs_it)->GetTXOC();
        result_txoc.Merge(std::move(txoc));
    }

    std::unordered_map<uint256, UTXOPtr> result_created{};
    for (const auto& key_created : result_txoc.GetCreated()) {
        result_created.emplace(key_created, ledger_.FindFromLedger(key_created));
    }
    return {result_rec, result_created, result_txoc.GetSpent()};
}

bool Chain::IsMilestone(const uint256& blkHash) const {
    auto result = recordHistory_.find(blkHash);
    if (result == recordHistory_.end()) {
        return CAT->IsMilestone(blkHash);
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
