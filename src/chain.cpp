#include "chain.h"
#include "caterpillar.h"
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
        for (const auto& h : (*it)->GetRecordHashes()) {
            pendingBlocks_.insert({h, recordHistory_.at(h)->cblock});
            recordHistory_.erase(h);
            ledger_.Rollback((*it)->GetTXOC());
        }
        states_.erase(std::next(it).base());
    }
    // note that we don't do any verification here
}

ChainStatePtr Chain::GetChainHead() const {
    if (states_.empty()) {
        uint64_t height = CAT->GetHeadHeight();
        return CAT->GetMilestoneAt(height)->snapshot;
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
        pendingUTXOs_.emplace(u->GetKey(), u);
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

bool Chain::IsValidDistance(const NodeRecord& b, const arith_uint256& ms_hashrate) {
    if (b.minerChainHeight <= GetParams().sortitionThreshold) {
        return !(b.cblock->HasTransaction());
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

    // Distance of the transaction hash and previous block hash
    auto dist = UintToArith256(b.cblock->GetTxHash()) ^ UintToArith256(b.cblock->GetPrevHash());

    auto nodeHandler = cumulatorMap_.extract(b.cblock->GetPrevHash());
    Cumulator& cum   = nodeHandler.mapped();

    // Allowed distance
    auto allowed = (cum.Sum() / (cum.TimeSpan() + 1)) / GetParams().sortitionCoefficient *
                   (GetParams().maxTarget / (ms_hashrate + 1));

    // Update key for the cumulator
    cum.Add(b.cblock, true);
    nodeHandler.key() = b.cblock->GetHash();
    cumulatorMap_.insert(std::move(nodeHandler));

    return dist <= allowed;
}

RecordPtr Chain::Verify(const ConstBlockPtr& pblock) {
    spdlog::trace("Verifying block {}", pblock->GetHash().to_substr());

    // get a path for validation by the post ordered DFS search
    std::vector<ConstBlockPtr> blocksToValidate = GetSortedSubgraph(pblock);

    std::vector<RecordPtr> recs;
    std::vector<uint256> hashes;
    recs.reserve(blocksToValidate.size());
    hashes.reserve(blocksToValidate.size());
    verifying_.clear();

    for (const auto& b : blocksToValidate) {
        recs.emplace_back(std::make_shared<NodeRecord>(b));
        hashes.emplace_back(b->GetHash());
    }
    auto state = CreateNextChainState(GetChainHead(), *recs.back(), std::move(hashes));

    // validate each block in order
    for (auto& rec : recs) {
        if (rec->cblock->IsFirstRegistration()) {
            const auto& blkHash = rec->cblock->GetHash();
            rec->prevRedemHash  = blkHash;
            rec->isRedeemed     = NodeRecord::NOT_YET_REDEEMED;
            state->regChange.Create(blkHash, blkHash);
            rec->minerChainHeight = 1;
        } else {
            if (auto update = Validate(*rec, state->regChange)) {
                rec->validity = NodeRecord::VALID;
                // update ledger in chain for future reference
                ledger_.Update(*update);
                // take notes in chain state; will be used when flushing this state from memory to CAT
                state->UpdateTXOC(std::move(*update));
            } else {
                rec->validity = NodeRecord::INVALID;
                TXOC invalid  = CreateTXOCFromInvalid(*(rec->cblock));
                // remove utxo of the block from pending to removed
                ledger_.Invalidate(invalid);
                // still take notes in chain state
                state->UpdateTXOC(std::move(invalid));
            }
            rec->UpdateReward(GetPrevReward(*rec));
        }
        rec->height = state->height;
        verifying_.insert({rec->cblock->GetHash(), rec});
    }

    recordHistory_.merge(std::move(verifying_));
    return recs.back();
}

std::optional<TXOC> Chain::Validate(NodeRecord& record, RegChange& regChange) {
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
        } else {
            result = ValidateTx(record);
        }
    } else {
        // regarded as a valid transaction but not updating ledger
        result = std::make_optional<TXOC>();
    }

    return result;
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

    const auto& redem = record.cblock->GetTransaction();
    const auto& vin   = redem->GetInputs().at(0);
    const auto& vout  = redem->GetOutputs().at(0); // only first tx output will be regarded as valid

    // value of the output should be less or equal to the previous counter
    if (!(vout.value <= prevReg->cumulativeReward)) {
        spdlog::info("Wrong redemption value that exceeds total cumulative reward! [{}]", hashstr);
        return {};
    }

    if (!VerifyInOut(vin, prevReg->cblock->GetTransaction()->GetOutputs()[0].listingContent)) {
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

    return TXOC{{XOR(record.cblock->GetHash(), 0)}, {}};
}

std::optional<TXOC> Chain::ValidateTx(NodeRecord& record) {
    spdlog::trace("Validating tx {}", record.cblock->GetHash().to_substr());

    const auto& tx      = record.cblock->GetTransaction();
    const auto& hashstr = std::to_string(record.cblock->GetHash());

    // check Transaction distance
    RecordPtr prevMs = DAG->GetState(record.cblock->GetMilestoneHash());
    assert(prevMs);
    if (!IsValidDistance(record, prevMs->snapshot->hashRate)) {
        spdlog::info("Transaction distance exceeds its allowed distance! [{}]",
                     std::to_string(record.cblock->GetHash()));
        return {};
    }

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
        auto prevOut = ledger_.FindSpendable(XOR(outpoint.bHash, outpoint.index));

        if (!prevOut) {
            spdlog::info("Attempting to spend a non-existent or spent output {} [{}]", std::to_string(outpoint),
                         hashstr);
            return {};
        }
        valueIn += prevOut->GetOutput().value;

        prevOutListing.emplace_back(prevOut->GetOutput().listingContent);
        txoc.AddToSpent(vin);
    }

    // get key of new UTXO and compute total value out
    for (size_t i = 0; i < tx->GetOutputs().size(); i++) {
        valueOut += tx->GetOutputs()[i].value;
        txoc.AddToCreated(record.cblock->GetHash(), i);
    }

    // check total amount of value in and value out and take a note of fee received
    Coin fee = valueIn - valueOut;
    if (!(fee >= 0 && fee <= GetParams().maxMoney)) {
        spdlog::info("Transaction input value goes out of range! [{}]", hashstr);
        return {};
    }
    record.fee = fee;

    // verify transaction input one by one
    auto itprevOut = prevOutListing.cbegin();
    for (const auto& input : tx->GetInputs()) {
        if (!VerifyInOut(input, *itprevOut)) {
            spdlog::info("Singature failed! [{}]", hashstr);
            return {};
        }
        itprevOut++;
    }
    return std::make_optional<TXOC>(std::move(txoc));
}

RecordPtr Chain::GetRecord(const uint256& blkHash) const {
    auto result = verifying_.find(blkHash);
    if (result == verifying_.end()) {
        result = recordHistory_.find(blkHash);
        if (result == recordHistory_.end()) {
            auto rec = CAT->GetRecord(blkHash);
            if (rec) {
                rec->prevRedemHash = CAT->GetPrevRedemHash(blkHash);
            }

            return rec;
        }
    }
    return result->second;
}

RecordPtr Chain::GetMsRecordCache(const uint256& msHash) {
    auto entry = recordHistory_.find(msHash);
    if (entry != recordHistory_.end() && entry->second->isMilestone) {
        return entry->second;
    }
    return nullptr;
}

RecordPtr Chain::GetRecordCache(const uint256& h) {
    auto entry = recordHistory_.find(h);
    if (entry != recordHistory_.end()) {
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

std::tuple<std::vector<std::vector<RecordPtr>>, std::unordered_map<uint256, UTXOPtr>, std::unordered_set<uint256>>
Chain::GetDataToCAT(size_t level) {
    std::vector<std::vector<RecordPtr>> result_rec{};
    result_rec.reserve(level);
    TXOC result_txoc{};

    // traverse from the oldest chain state in memory 
    for (auto cs_it = states_.begin(); cs_it < states_.begin() + level && cs_it != states_.end(); cs_it++) {
        std::vector<RecordPtr> lvsRec;
        lvsRec.reserve((*cs_it)->GetRecordHashes().size());

        for (const auto& h : (*cs_it)->GetRecordHashes()) {
            lvsRec.emplace_back(recordHistory_.at(h));
        }
        result_rec.emplace_back(std::move(lvsRec));
        // TODO: optimize it after merging increament
        result_txoc.Merge(std::move(const_cast<TXOC&>((*cs_it)->GetTXOC())));
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
