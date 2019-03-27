#include <string>
#include <iostream>

#include "sha256.h"
#include "spdlog/spdlog.h"
#include "tinyformat.h"
#include "transaction.h"
#include "init.h"


int main(int argc, char *argv[]) {
    Init(argc, argv);

    spdlog::info("Welcome to epic, enjoy your time!");

    uint256 zeros = uint256S(new char[256]());
    TxOutPoint outpoint = TxOutPoint(zeros, 1);
    TxInput input = TxInput(outpoint, Script());
    TxOutput output = TxOutput(100, Script());
    Transaction tx = Transaction(1);
    tx.AddInput(input);
    tx.AddOutput(output);

    std::cout << strprintf("A transaction: \n %s", tx.ToString()) << std::endl;
}
