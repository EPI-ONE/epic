#include "transaction.h"

using Listing = Tasm::Listing;

/*
 * TxInput class START
 */

TxInput::TxInput(const TxOutPoint& outpointToPrev, const Tasm::Listing& listingData) {
    outpoint       = outpointToPrev;
    listingContent = listingData;
}

TxInput::TxInput(const uint256& fromBlockHash, uint32_t indexNum, const Tasm::Listing& listingData) {
    outpoint       = TxOutPoint(fromBlockHash, indexNum);
    listingContent = listingData;
}

TxInput::TxInput(const Tasm::Listing& listingData) {
    outpoint       = TxOutPoint(Hash::GetZeroHash(), UNCONNECTED);
    listingContent = listingData;
}

bool TxInput::IsRegistration() const {
    return outpoint.index == UNCONNECTED;
}

bool TxInput::IsFirstRegistration() const {
    return outpoint.bHash == Hash::GetZeroHash() && IsRegistration();
}

void TxInput::SetParent(const Transaction* const tx) {
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

void TxOutput::SetParent(const Transaction* const tx) {
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
    hash_.SetNull();
    FinalizeHash();
    SetParents();
}

Transaction::Transaction(const CKeyID& addr) {
    AddInput(TxInput{Listing{}}).AddOutput(Coin{}, addr);
    FinalizeHash();
}

void Transaction::SetParents() {
    for (TxInput& input : inputs_) {
        input.SetParent(this);
    }
    for (TxOutput& output : outputs_) {
        output.SetParent(this);
    }
}

Transaction& Transaction::AddInput(TxInput&& txin) {
    hash_.SetNull();
    txin.SetParent(this);
    inputs_.push_back(txin);
    return *this;
}

Transaction& Transaction::AddSignedInput(const TxOutPoint& outpoint,
                                         const CPubKey& pubkey,
                                         const uint256& hashMsg,
                                         const std::vector<unsigned char>& sig) {
    VStream indata{pubkey, sig, hashMsg};
    AddInput(TxInput{outpoint, Tasm::Listing{indata}});
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
    VStream vstream{EncodeAddress(addr)};
    return AddOutput(TxOutput{coin, Listing{std::vector<uint8_t>{VERIFY}, std::move(vstream)}});
}

void Transaction::FinalizeHash() {
    if (hash_.IsNull()) {
        hash_ = Hash<1>(VStream(*this));
    }
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

void Transaction::SetParent(const Block* const blk) {
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
    Tasm tasm(functors);
    return bool(tasm.ExecListing(Listing{input.listingContent + outputListing}));
}

/*
 * to_string methods START
 */

std::string std::to_string(const TxOutPoint& outpoint) {
    std::string str;
    str += strprintf("%s:%d", std::to_string(outpoint.bHash), outpoint.index);
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
