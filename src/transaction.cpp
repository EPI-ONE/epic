#include "transaction.h"

/*
 * TxInput class START
 */

TxInput::TxInput(const TxOutPoint& outpointToPrev, const Tasm::Listing& listingData) {
    outpoint       = outpointToPrev;
    listingContent = listingData;
}

TxInput::TxInput(const uint256& fromBlockHash, const uint32_t indexNum, const Tasm::Listing& listingData) {
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

const Transaction* TxInput::GetParentTx() const{
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

Transaction::Transaction() {
    inputs  = std::vector<TxInput>();
    outputs = std::vector<TxOutput>();
    status_ = UNKNOWN;
}

Transaction::Transaction(const Transaction& tx) {
    hash_.SetNull();
    inputs       = tx.inputs;
    outputs      = tx.outputs;
    fee_         = tx.fee_;
    parentBlock_ = tx.parentBlock_;
    status_      = tx.status_;
    FinalizeHash();

    // Set parents for the copied inputs and outputs
    for (TxInput& input : inputs) {
        input.SetParent(this);
    }

    for (TxOutput& output : outputs) {
        output.SetParent(this);
    }
}

Transaction& Transaction::AddInput(TxInput&& txin) {
    hash_.SetNull();
    txin.SetParent(this);
    inputs.push_back(txin);
    return *this;
}

Transaction& Transaction::AddOutput(TxOutput&& txout) {
    hash_.SetNull();
    txout.SetParent(this);
    outputs.push_back(txout);
    return *this;
}

void Transaction::FinalizeHash() {
    if (hash_.IsNull())
        hash_ = Hash<1>(VStream(*this));
}

const TxInput& Transaction::GetInput(size_t index) const {
    return inputs.at(index);
}

const TxOutput& Transaction::GetOutput(size_t index) const {
    return outputs.at(index);
}

const std::vector<TxInput>& Transaction::GetInputs() const {
    return inputs;
}

const std::vector<TxOutput>& Transaction::GetOutputs() const {
    return outputs;
}

const Tasm::Listing Transaction::GetListing() const {
    Tasm::Listing l;
    if (inputs.size() > 1 && outputs.size() > 1) {
        throw std::runtime_error("Transaction is inconsistent. Input size and output size currently supported is <= 1");
    }

    if (inputs.size() == 1 && outputs.size() == 1) {
        l.program.push_back(VERIFY);
        l.data << outputs[0].listingContent.data;
        l.data << inputs[0].listingContent.data;
    }

    return l;
}

std::vector<TxInput>& Transaction::GetInputs() {
    return inputs;
}

std::vector<TxOutput>& Transaction::GetOutputs() {
    return outputs;
}

const uint256& Transaction::GetHash() const {
    return hash_;
}

bool Transaction::IsRegistration() const {
    return inputs.size() == 1 && inputs.front().IsRegistration();
}

bool Transaction::IsFirstRegistration() const {
    return inputs.size() == 1 && inputs.front().IsFirstRegistration() && outputs.front().value == ZERO_COIN;
}

bool Transaction::Verify() const {
    if (inputs.empty() || outputs.empty()) {
        spdlog::info("Empty inputs or outputs");
        return false;
    }

    // Make sure no duplicated TxInput
    std::unordered_set<TxOutPoint> outpoints = {};
    for (const TxInput& input : inputs) {
        if (outpoints.find(input.outpoint) != outpoints.end()) {
            spdlog::info("Duplicated outpoint found in inputs");
            return false;
        }
        outpoints.insert(input.outpoint);
    }

    outpoints.clear();

    return true;
}

void Transaction::Validate() {
    status_ = VALID;
}

void Transaction::Invalidate() {
    status_ = INVALID;
}

void Transaction::SetStatus(Transaction::Validity&& status) {
    status_ = status;
}

Transaction::Validity Transaction::GetStatus() const {
    return status_;
}

void Transaction::SetParent(const Block* const blk) {
    assert(blk != nullptr);
    parentBlock_ = blk;
}

const Block* Transaction::GetParentBlock() const {
    return parentBlock_;
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
    str += "TxInput{ ";

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
    str += "TxOut{ ";
    str += strprintf("value=%d, listing content = %s", output.value.GetValue(), std::to_string(output.listingContent));

    return str += " }";
}

std::string std::to_string(Transaction& tx) {
    std::string s;
    s += "Transaction { \n";
    s += strprintf("   hash: %s \n", std::to_string(tx.GetHash()));

    for (int i = 0; i < tx.GetInputs().size(); ++i) {
        s += "   ";
        s += std::to_string(tx.GetInputs()[i]);
        s += "\n";
    }

    for (int j = 0; j < tx.GetOutputs().size(); ++j) {
        s += "   ";
        s += std::to_string(tx.GetOutputs()[j]);
        s += "\n";
    }

    s += " }\n";
    return s;
}
