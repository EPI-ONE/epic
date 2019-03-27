#include "transaction.h"
#include "tinyformat.h"

// TODO: think carefully about where to use pointers

std::string TxOutPoint::ToString() const {
    std::string str;
    str += strprintf("%s:%d", hash.ToString(), index);
    return str;
}

TxInput::TxInput(const TxOutPoint& outpointToPrev, const Script& script) {
    outpoint  = outpointToPrev;
    scriptSig = script;
}

TxInput::TxInput(const uint256& fromBlockHash, const uint32_t indexNum, const Script& script) {
    outpoint  = TxOutPoint(fromBlockHash, indexNum);
    scriptSig = script;
}

TxInput::TxInput(const Script& script) {
    outpoint  = TxOutPoint(Hash::ZERO_HASH, NEGATIVE_ONE);
    scriptSig = script;
}

std::string TxInput::ToString() const {
    std::string str;
    str += "TxInput{ ";
    if (IsRegistration()) {
        str += "REGISTRATION";
    } else {
        str += strprintf("outpoint=%s, scriptSig=%s", outpoint.ToString(), scriptSig.ToString());
    }
    str += " }";
    return str;
}

TxOutput::TxOutput(const Coin& coinValue, const Script& script) {
    value        = coinValue;
    scriptPubKey = script;
}

std::string TxOutput::ToString() const {
    std::string str;
    str += "TxOut{ ";
    str += strprintf("value=%d, scriptPubKey=%s", value, scriptPubKey.ToString());
    str += " }";
    return str;
}

Transaction::Transaction(const Transaction& tx) {
    hash_.SetNull();
    inputs       = tx.inputs;
    outputs      = tx.outputs;
    fee_         = tx.fee_;
    parentBlock_ = tx.parentBlock_;
    status_      = tx.status_;
}

void Transaction::AddInput(TxInput&& txin) {
    hash_.SetNull();
    txin.SetParent(*this);
    inputs.push_back(txin);
}

void Transaction::AddOutput(TxOutput&& txout) {
    hash_.SetNull();
    txout.SetParent(*this);
    outputs.push_back(txout);
}

std::string Transaction::ToString() {
    std::string s;
    s += "Transaction { \n";
    s += strprintf("   hash: %s \n", GetHash().ToString());
    for (int i = 0; i < inputs.size(); ++i) {
        s += "   ";
        s += inputs.at(i).ToString();
        s += "\n";
    }
    for (int j = 0; j < outputs.size(); ++j) {
        s += "   ";
        s += outputs.at(j).ToString();
        s += "\n";
    }
    s += " }\n";
    return s;
}
