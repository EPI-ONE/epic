#include <iostream>
#include <string>
#include <unordered_map>

#include "arith_uint256.h"
#include "block.h"
#include "file_utils.h"
#include "hash.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "sha256.h"
#include "spdlog/spdlog.h"
#include "tinyformat.h"
#include "transaction.h"
#include "init.h"


int main(int argc, char *argv[]) {
    Init(argc, argv);

    spdlog::info("Welcome to epic, enjoy your time!");

    TxOutPoint outpoint = TxOutPoint(ZERO_HASH, 1);
    TxInput input = TxInput(outpoint, Script());
    TxOutput output = TxOutput(100, Script());
    Transaction tx = Transaction(1);
    tx.AddInput(input);
    tx.AddOutput(output);

    arith_uint256 zeros = UintToArith256(ZERO_HASH);
    std::cout << strprintf("ZERO_HASH: \n %s", zeros.ToString()) << std::endl;
    std::cout << strprintf("A transaction: \n %s", tx.ToString()) << std::endl;

    std::unordered_map<uint256, uint256> testMap = {{ZERO_HASH, ZERO_HASH}};
    std::cout << strprintf("An unordered_map: \n %s", testMap) << std::endl;
}
