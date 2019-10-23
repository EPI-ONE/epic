// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef EPIC_TEST_ENV_H
#define EPIC_TEST_ENV_H

#include <gtest/gtest.h>

#include "block_store.h"
#include "miner.h"
#include "params.h"
#include "test_factory.h"
#include "wallet.h"

#include <memory>

class EpicTestEnvironment : public ::testing::Environment {
public:
    static const TestFactory& GetFactory() {
        static const TestFactory fac{};
        return fac;
    }

    void SetUp() override {
        ECC_Start();
        handle = ECCVerifyHandle();
        SelectParams(ParamsType::UNITTEST);
    }

    void TearDown() override {
        ECC_Stop();
    }

    static void SetUpDAG(std::string dirPath, bool enable_miner = false, bool enable_wallet = false) {
        std::ostringstream os;
        os << time(nullptr);
        file::SetDataDirPrefix(dirPath + os.str());
        STORE = std::make_unique<BlockStore>(dirPath + os.str());
        DAG   = std::make_unique<DAGManager>();

        // Initialize DB with genesis
        std::vector<VertexPtr> genesisLvs = {GENESIS_VERTEX};
        STORE->StoreLevelSet(genesisLvs);

        if (enable_miner) {
            MEMPOOL = std::make_unique<MemPool>();
            MINER   = std::make_unique<Miner>(4);
        }

        if (enable_wallet) {
            WALLET = std::make_unique<Wallet>(dirPath + "/data/", 1, 0);
            DAG->RegisterOnLvsConfirmedCallback(
                [&](auto vec, auto map1, auto map2) { WALLET->OnLvsConfirmed(vec, map1, map2); });
        }
    }

    static void TearDownDAG(const std::string& dirPath) {
        if (STORE) {
            STORE->Wait();
            STORE->Stop();
        }
        if (DAG) {
            DAG->Stop();
        }
        if (WALLET) {
            WALLET->Stop();
        }

        STORE.reset();
        DAG.reset();
        MEMPOOL.reset();
        MINER.reset();
        WALLET.reset();

        std::string cmd = "exec rm -r " + dirPath;
        system(cmd.c_str());
    }

private:
    ECCVerifyHandle handle;
};

inline void SetLogLevel(int level) {
    spdlog::set_level((spdlog::level::level_enum) level);
}

inline void ResetLogLevel() {
    SetLogLevel(SPDLOG_LEVEL_INFO);
}

#endif // EPIC_TEST_ENV_H
