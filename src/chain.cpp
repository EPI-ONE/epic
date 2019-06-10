#include "caterpillar.h"
#include "chain.h"
#include "tasm/functors.h"
#include "tasm/tasm.h"

Chain::Chain(bool mainchain) : ismainchain_(mainchain) {
    pendingBlocks_ = {};
    recordHistory_ = {};
    states_        = {std::make_shared<ChainState>()};
}

ChainStatePtr Chain::GetChainHead() const {
    return states_.back();
}

Chain::Chain(const Chain& chain) : ismainchain_(false), states_(chain.states_), pendingBlocks_(chain.pendingBlocks_) {}

Chain::Chain(const Chain& chain, ConstBlockPtr pfork)
    : ismainchain_(false), states_(chain.states_), pendingBlocks_(chain.pendingBlocks_),
      recordHistory_(chain.recordHistory_), ledger_(chain.ledger_) {
    ChainStatePtr cursor = chain.GetChainHead();
    uint256 target       = pfork->GetMilestoneHash();
    assert(recordHistory_.at(target));

    for (auto it = states_.rbegin(); (*it)->GetMilestoneHash() != target && it != states_.rend(); it++) {
        for (const auto& h : (*it)->GetRecordHashes()) {
            pendingBlocks_.insert({h, recordHistory_[h]->cblock});
            recordHistory_.erase(h);
            ledger_.Rollback((*it)->GetTXOC());
        }
        states_.erase(std::next(it).base());
    }
    // note that we don't do any verification here
}

void Chain::AddPendingBlock(ConstBlockPtr pblock) {
    pendingBlocks_.insert_or_assign(pblock->GetHash(), pblock);
}

void Chain::RemovePendingBlock(const uint256& hash) {
    pendingBlocks_.erase(hash);
}

bool Chain::IsBlockPending(const uint256& hash) const {
    return pendingBlocks_.find(hash) != pendingBlocks_.end();
}

std::size_t Chain::GetPendingBlockCount() const {
    return pendingBlocks_.size();
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
    auto current_distance = UintToArith256(b.cblock->GetTxHash()) ^ UintToArith256(b.cblock->GetPrevHash());
    arith_uint256 S       = 0;
    arith_uint256 t       = 0;

    auto curr = CAT->GetRecord(b.cblock->GetPrevHash());
    for (size_t i = 0; i < params.sortitionThreshold && *curr != GENESIS_RECORD;
         ++i, curr = CAT->GetRecord(curr->cblock->GetPrevHash())) {
        S += curr->cblock->GetChainWork();
        t = curr->cblock->GetTime();
    }

    auto allowed_distance = (params.maxTarget / params.sortitionCoefficient) * S / (ms_hashrate * t);
    return current_distance <= allowed_distance;
}

void Chain::Verify(const ConstBlockPtr& pblock) {
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
    auto state = make_shared_ChainState(GetChainHead(), *recs.back(), std::move(hashes));

    // validate each block in order
    for (auto& rec : recs) {
        if (rec->cblock->IsFirstRegistration()) {
            rec->prevRedemHash = rec->cblock->GetHash();
        } else {
            if (auto update = Validate(*rec)) {
                ledger_.Update(*update);
                state->UpdateTXOC(std::move(*update));
            } else {
                rec->validity = NodeRecord::INVALID;
                spdlog::info("Transaction in block {} is invalid!", std::to_string(rec->cblock->GetHash()));
            }
        }
        verifying_.insert({rec->cblock->GetHash(), rec});
    }
    states_.emplace_back(state);
    recordHistory_.merge(std::move(verifying_));
}

std::optional<TXOC> Chain::Validate(NodeRecord& record) {
    auto pblock = record.cblock;
    std::optional<TXOC> result;

    // first check whether it is a fork of its peer chain
    bool validPeerChain = false;
    auto prevRec        = GetRecord(record.cblock->GetPrevHash());
    if (prevRec->prevRedemHash != Hash::GetZeroHash()) {
        // then its previous block is valid in the sense of reward
        record.prevRedemHash   = prevRec->prevRedemHash;
        prevRec->prevRedemHash = Hash::GetZeroHash();
        validPeerChain         = true;
    } else {
        record.prevRedemHash = Hash::GetZeroHash();
    }

    // then check its transaction and update UTXO
    if (pblock->HasTransaction()) {
        if (pblock->IsRegistration()) {
            result = ValidateRedemption(record);
        } else {
            result = ValidateTx(record);
        }
    }

    record.UpdateReward(GetPrevReward(record), validPeerChain);
    return result;
}

std::optional<TXOC> Chain::ValidateRedemption(NodeRecord& record) {
    // this transaction is a redemption of reward
    const auto redem = record.cblock->GetTransaction();
    if (redem->GetOutputs().empty()) {
        spdlog::info("Missing output for redemption of reward!");
        return {};
    }
    const TxInput& vin   = redem->GetInputs().at(0);
    const TxOutput& vout = redem->GetOutputs().at(0); // only first tx output will be regarded as valid

    // value of the output should be less or equal to the previous counter
    if (!(GetPrevReward(record) <= vout.value)) {
        spdlog::info("Wrong redemption value that exceeds total cumulative reward!");
        return {};
    }

    auto prevReg = GetRecord(record.prevRedemHash);
    assert(prevReg);
    if (prevReg->isRedeemed != NodeRecord::NOT_YET_REDEEMED) {
        spdlog::info("Double redemption on previous registration block {}", std::to_string(record.prevRedemHash));
        return {};
    }

    if (!VerifyInOut(vin, prevReg->cblock->GetTransaction()->GetOutputs()[0].listingContent)) {
        spdlog::info("Singature failed!");
        return {};
    }

    // update redemption status
    prevReg->isRedeemed = NodeRecord::IS_REDEEMED;
    record.isRedeemed   = NodeRecord::NOT_YET_REDEEMED;
    return std::make_optional<TXOC>({{XOR(record.cblock->GetHash(), 0)}, {}});
}

std::optional<TXOC> Chain::ValidateTx(NodeRecord& record) {
    auto tx = record.cblock->GetTransaction();

    // check Transaction distance
    ChainStatePtr prevMs = DAG->GetState(record.cblock->GetMilestoneHash());
    assert(prevMs);
    if (!IsValidDistance(record, prevMs->hashRate)) {
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
        const TxOutPoint& outpoint    = vin.outpoint;
        // this ensures that $prevOut has not been spent yet
        auto prevOut = ledger_.FindSpendable(XOR(outpoint.bHash, outpoint.index));

        if (!prevOut) {
            spdlog::info("Attempting to spend a non-existent or spent output {}", std::to_string(outpoint));
            return {};
        }
        valueIn = valueIn + prevOut->GetOutput().value;

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
    if (!(fee >= 0 && fee <= params.maxMoney && valueIn <= params.maxMoney)) {
        spdlog::info("Transaction input vlaue goes out of range!");
        return {};
    }
    record.fee = fee;

    // verify transaction input one by one
    auto itprevOut = prevOutListing.cbegin();
    for (const auto& input : tx->GetInputs()) {
        if (!VerifyInOut(input, *itprevOut)) {
            spdlog::info("Singature failed!");
            return {};
        }
        itprevOut++;
    }
    return std::make_optional<TXOC>(std::move(txoc));
}

RecordPtr Chain::GetRecord(const uint256& blkHash) const {
    try {
        if (verifying_.find(blkHash) != verifying_.end()) {
            return verifying_.at(blkHash);
        } else if (recordHistory_.find(blkHash) != recordHistory_.end()) {
            return recordHistory_.at(blkHash);
        } else {
            return CAT->GetRecord(blkHash);
        }
    } catch (std::out_of_range& e) {
        return nullptr;
    }
}
