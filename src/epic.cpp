// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "init.h"
#include "spdlog/spdlog.h"

int main(int argc, char** argv) {
    int init_result = Init(argc, argv);
    if (!init_result) {
        CreateDaemon();
    } else {
        return init_result;
    }

    if (Start()) {
        // TODO: start some applications, such as miner, visualization...
        WaitShutdown();
    } else {
        std::cout << "Failed to start epic" << std::endl;
    }
    ShutDown();
    return NORMAL_EXIT;
}
