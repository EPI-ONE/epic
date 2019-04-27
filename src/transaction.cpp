#include "transaction.h"

/*
 * TxInput class START
 */

TxInput::TxInput(const TxOutPoint& outpointToPrev, const Script& script) {
    outpoint  = outpointToPrev;
    scriptSig = script;
}

TxInput::TxInput(const uint256& fromBlockHash, const uint32_t indexNum, const Script& script) {
    outpoint  = TxOutPoint(fromBlockHash, indexNum);
    scriptSig = script;
}

TxInput::TxInput(const Script& script) {
    outpoint  = TxOutPoint(Hash::ZERO_HASH, UNCONNECTED);
    scriptSig = script;
}

bool TxInput::IsRegistration() const {
    return outpoint.index == UNCONNECTED;
}

bool TxInput::IsFirstRegistration() const {
    return outpoint.bHash == Hash::ZERO_HASH && IsRegistration();
}


void TxInput::SetParent(const Transaction* const tx) {
    assert(tx != nullptr);
    parentTx_ = tx;
}

const Transaction* TxInput::GetParentTx() {
    return parentTx_;
}

/*
 * TxOutput class START
 */

TxOutput::TxOutput() {
    value = IMPOSSIBLE_COIN;
    scriptPubKey.clear();
}

TxOutput::TxOutput(const Coin& coinValue, const Script& script) {
    value        = coinValue;
    scriptPubKey = script;
}

void TxOutput::SetParent(const Transaction* const tx) {
    assert(tx != nullptr);
    parentTx_ = tx;
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

bool Transaction::Verify() const {
    if (inputs.empty() || outputs.empty()) {
        return false;
    }

    // Make sure no duplicated TxInput
    std::unordered_set<TxOutPoint> outpoints = {};
    for (const TxInput& input : inputs) {
        if (outpoints.find(input.outpoint) != outpoints.end()) {
            return false;
        }
        outpoints.insert(input.outpoint);
    }

    outpoints.clear();

    // Make sure that there aren't not too many sig verifications
    // in the block to prevent the potential DDos attack.
    int sigOps = 0;
    for (const TxInput& input : inputs) {
        sigOps += Script::GetSigOpCount(input.scriptSig);
    }

    if (sigOps > MAX_BLOCK_SIGOPS) {
        return false;
    }

    return true;
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
        str += "REGISTRATION";
    } else {
        str += strprintf("outpoint=%s, scriptSig=%s", std::to_string(input.outpoint), std::to_string(input.scriptSig));
    }

    return str + " }";
}

std::string std::to_string(const TxOutput& output) {
    std::string str;
    str += "TxOut{ ";
    str += strprintf("value=%d, scriptPubKey=%s", output.value, std::to_string(output.scriptPubKey));

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
