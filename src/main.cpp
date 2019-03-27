#include <string>
#include <iostream>

#include "sha256.h"
#include "spdlog/spdlog.h"
#include "tinyformat.h"
#include "transaction.h"
#include "init.h"
#include "hash.h"
#include "block.h"
#include "file_utils.h"
#include "crypto/sha256.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/spdlog.h"
#include "tinyformat.h"
#include "transaction.h"
#include "utils/cpptoml.h"
#include "arith_uint256.h"


class uint256;
//class arith_uint256;


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

    arith_uint256 zeroos = UintToArith256(zeros);
    std::cout << strprintf("A null sha: \n %s", zeroos.ToString()) << std::endl;

    std::cout << strprintf("A transaction: \n %s", tx.ToString()) << std::endl;
}
