#include "transaction.h"
#include "block.h"
#include "params.h"
#include "spdlog.h"

#include <string>
#include <unordered_set>

using Listing = Tasm::Listing;

uint256 ComputeUTXOKey(const uint256& hash, uint32_t txIndex, uint32_t outIndex) {
    return ArithToUint256(UintToArith256(hash) ^ (arith_uint256(txIndex) << 224) ^ (arith_uint256(outIndex) << 192));
}

/*
 * TxInput class START
 */
bool TxInput::IsRegistration() const {
    return outpoint.txIndex == UNCONNECTED && outpoint.outIndex == UNCONNECTED;
}

bool TxInput::IsFirstRegistration() const {
    return outpoint.bHash == Hash::GetZeroHash() && IsRegistration();
}

void TxInput::SetParent(const Transaction* const tx) const {
    assert(tx != nullptr);
    parentTx_ = tx;
}

const Transaction* TxInput::GetParentTx() const {
    return parentTx_;
}

/*
 * TxOutput class START
 */

TxOutput::TxOutput() {
    value = IMPOSSIBLE_COIN;
}

TxOutput::TxOutput(const Coin& coinValue, const Tasm::Listing& listingData) {
    value          = coinValue;
    listingContent = listingData;
}

TxOutput::TxOutput(const uint64_t& coinValue, const Tasm::Listing& listingData) {
    value          = coinValue;
    listingContent = listingData;
}

void TxOutput::SetParent(const Transaction* const tx) const {
    assert(tx != nullptr);
    parentTx_ = tx;
}

const Transaction* TxOutput::GetParentTx() const {
    return parentTx_;
}

/*
 * transaction class START
 */

Transaction::Transaction(const Transaction& tx) {
    hash_.SetNull();
    inputs_      = tx.inputs_;
    outputs_     = tx.outputs_;
    parentBlock_ = tx.parentBlock_;
    FinalizeHash();
    SetParents();
}

Transaction::Transaction(Transaction&& tx) noexcept
    : inputs_(std::move(tx.inputs_)), outputs_(std::move(tx.outputs_)), parentBlock_(tx.parentBlock_) {
    tx.parentBlock_ = nullptr;
    hash_.SetNull();
    FinalizeHash();
    SetParents();
}

Transaction::Transaction(const CKeyID& addr) {
    AddInput(TxInput{}).AddOutput(Coin{}, addr);
    FinalizeHash();
    SetParents();
}

Transaction::Transaction(CKeyID& addr) : Transaction((const CKeyID) addr) {}

void Transaction::SetParents() const {
    for (const TxInput& input : inputs_) {
        input.SetParent(this);
    }
    for (const TxOutput& output : outputs_) {
        output.SetParent(this);
    }
}

Transaction& Transaction::AddInput(TxInput&& txin) {
    hash_.SetNull();
    txin.SetParent(this);
    inputs_.push_back(txin);
    return *this;
}

Transaction& Transaction::AddOutput(TxOutput&& txout) {
    hash_.SetNull();
    txout.SetParent(this);
    outputs_.push_back(txout);
    return *this;
}

Transaction& Transaction::AddOutput(uint64_t value, const CKeyID& addr) {
    Coin coin{value};
    return AddOutput(coin, addr);
}

Transaction& Transaction::AddOutput(const Coin& coin, const CKeyID& addr) {
    VStream vstream;
    vstream << EncodeAddress(addr);
    return AddOutput(TxOutput{coin, Listing{std::vector<uint8_t>{VERIFY}, std::move(vstream)}});
}

void Transaction::FinalizeHash() {
    if (hash_.IsNull()) {
        hash_ = HashSHA2<1>(VStream(*this));
    }
}

bool Transaction::Verify() const {
    if (inputs_.empty() || outputs_.empty()) {
        spdlog::info("Transaction {} contains empty inputs or outputs [{}]", hash_.to_substr(),
                     std::to_string(parentBlock_->GetHash()));
        return false;
    }
    // TODO: add signature size checking

    // check no double spending on the same output
    std::unordered_set<TxOutPoint> outpoints;
    outpoints.reserve(inputs_.size());
    for (const auto& input : inputs_) {
        if (outpoints.count(input.outpoint) > 0) {
            spdlog::info("Transaction {} contains duplicated outpoints [{}]", hash_.to_substr(),
                         std::to_string(parentBlock_->GetHash()));
            return false;
        }
        outpoints.emplace(input.outpoint);
    }

    return true;
}

const std::vector<TxInput>& Transaction::GetInputs() const {
    return inputs_;
}

const std::vector<TxOutput>& Transaction::GetOutputs() const {
    return outputs_;
}

std::vector<TxInput>& Transaction::GetInputs() {
    return inputs_;
}

std::vector<TxOutput>& Transaction::GetOutputs() {
    return outputs_;
}

const uint256& Transaction::GetHash() const {
    return hash_;
}

bool Transaction::IsRegistration() const {
    return inputs_.size() == 1 && inputs_.front().IsRegistration();
}

bool Transaction::IsFirstRegistration() const {
    return inputs_.size() == 1 && inputs_.front().IsFirstRegistration() && outputs_.front().value == ZERO_COIN;
}

void Transaction::SetParent(const Block* const blk) const {
    assert(blk != nullptr);
    parentBlock_ = blk;
}

const Block* Transaction::GetParentBlock() const {
    return parentBlock_;
}

uint64_t Transaction::HashCode() const {
    assert(!hash_.IsNull());
    return hash_.GetCheapHash();
}

bool VerifyInOut(const TxInput& input, const Tasm::Listing& outputListing) {
    return Tasm().ExecListing(Listing(input.listingContent + outputListing));
}

/*
 * to_string methods START
 */

std::string std::to_string(const TxOutPoint& outpoint) {
    std::string str;
    str += strprintf("%s:%d,%d", std::to_string(outpoint.bHash), outpoint.txIndex, outpoint.outIndex);
    return str;
}

std::string std::to_string(const TxInput& input) {
    std::string str;
    str += "TxInput { ";

    if (input.IsRegistration()) {
        str += "REGISTRATION ";
        str += strprintf("listing content = %s", std::to_string(input.listingContent));
    } else {
        str += strprintf("outpoint = %s, listing content = %s", std::to_string(input.outpoint),
                         std::to_string(input.listingContent));
    }

    return str + " }";
}

std::string std::to_string(const TxOutput& output) {
    std::string str;
    str += "TxOut { ";
    str += strprintf("value=%d, listing content = %s", output.value.GetValue(), std::to_string(output.listingContent));

    return str += " }";
}

std::string std::to_string(const Transaction& tx) {
    std::string s;
    s += "Transaction { \n";
    s += strprintf("     hash: %s \n", std::to_string(tx.GetHash()));

    for (const auto& input : tx.GetInputs()) {
        s += "     ";
        s += std::to_string(input);
        s += "\n";
    }

    for (const auto& output : tx.GetOutputs()) {
        s += "     ";
        s += std::to_string(output);
        s += "\n";
    }

    s += "   }\n";
    return s;
}
