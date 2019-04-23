#include "transaction.h"
#include "tinyformat.h"

// TODO: think carefully about where to use pointers

std::string TxOutPoint::ToString() const {
    std::string str;
    str += strprintf("%s:%d", hash.ToString(), index);
    return str;
}

TxInput::TxInput(TxOutPoint outpointToPrev, Script script) {
    outpoint  = outpointToPrev;
    scriptSig = script;
}

TxInput::TxInput() {
}

TxInput::TxInput(uint256 fromBlockHash, uint32_t indexNum, Script script) {
    outpoint  = TxOutPoint(fromBlockHash, indexNum);
    scriptSig = script;
}

std::string TxInput::ToString() const {
    std::string str;
    str += "TxInput( ";
    if (isRegistration) {
        str += "REGISTRATION";
    } else {
        str += strprintf("outpoint=%s, scriptSig=%s", outpoint.ToString(), scriptSig.ToString());
    }
    str += " )";
    return str;
}

TxOutput::TxOutput(const Coin coinValue, Script script) {
    value        = coinValue;
    scriptPubKey = script;
}

std::string TxOutput::ToString() const {
    std::string str;
    str += "TxOut( ";
    str += strprintf("value=%d, scriptPubKey=%s", value, scriptPubKey.ToString());
    str += " )";
    return str;
}

Transaction::Transaction() {
}

Transaction::Transaction(const Transaction& tx) {
    inputs  = tx.inputs;
    outputs = tx.outputs;
    version = tx.version;
}

Transaction::Transaction(const std::vector<TxInput>& txInputs, const std::vector<TxOutput>& txOutputs) {
    version = 1;
    inputs  = txInputs;
    outputs = txOutputs;
}

Transaction::Transaction(uint32_t versionNum) {
    version = versionNum;
    inputs  = std::vector<TxInput>();
    outputs = std::vector<TxOutput>();
}

void Transaction::AddInput(TxInput& in) {
    inputs.push_back(in);
}

void Transaction::AddOutput(TxOutput& out) {
    outputs.push_back(out);
}

std::string Transaction::ToString() const {
    std::string str;
    str += "Transaction( \n";
    for (int i = 0; i < inputs.size(); ++i) {
        str += "   ";
        str += inputs.at(i).ToString();
        str += "\n";
    }
    for (int j = 0; j < outputs.size(); ++j) {
        str += "   ";
        str += outputs.at(j).ToString();
        str += "\n";
    }
    str += " )";
    return str;
}
