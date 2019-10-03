// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "mempool.h"
#include "miner.h"
#include "params.h"
#include "wallet.h"

#include <cstdlib>

int main(int argc, char* argv[]) {
    int nPeriods = 10;
    int startLen = 4;
    int endLen   = 42;

    if (argc > 1) {
        nPeriods = std::max(atoi(argv[1]), 2);
    }

    if (argc == 4) {
        startLen = std::max(atoi(argv[2]), 4);
        endLen   = std::max(atoi(argv[3]), 5);
    }

    std::string dataRoot = "diffStats/";
    std::string statsDir = dataRoot + "stats/";
    MkdirRecursive(statsDir);

    SelectParams(ParamsType::TESTNET);
    auto params           = GetParams();
    params.targetTimespan = 25;
    params.timeInterval   = 5;
    params.interval       = 5;

    auto genesis = *GENESIS_VERTEX->cblock;

    ECC_Start();
    auto handle = ECCVerifyHandle();

    MEMPOOL = std::make_unique<MemPool>();

    for (int i = startLen; i <= endLen; ++i) {
        if (i % 2) {
            // i must be even
            continue;
        }

        // Environment setup
        params.cycleLen = i;
        SetParams(params);

        genesis.SetProof(std::vector<word_t>(CYCLELEN));
        genesis.CalculateHash();
        genesis.CalculateOptimalEncodingSize();
        GENESIS                = std::make_shared<const Block>(genesis);
        GENESIS_VERTEX->cblock = GENESIS;

        DAG = std::make_unique<DAGManager>();

        auto index = tfm::format("%06d", i);
        auto dbDir = dataRoot + index;
        file::SetDataDirPrefix(dbDir);
        STORE = std::make_unique<BlockStore>(dbDir);
        STORE->StoreLevelSet((std::vector<VertexPtr>){GENESIS_VERTEX});
        WALLET = std::make_shared<Wallet>(dbDir + "/wallet", 1000);

        auto reg = WALLET->CreateFirstRegistration(WALLET->CreateNewKey(true));
        MEMPOOL->PushRedemptionTx(reg);

        Miner m(4);

        // Prepare csv file to write
        auto filename = statsDir + index + ".csv";
        std::ofstream buffer;
        buffer.open(filename);
        if (!buffer.is_open()) {
            std::cout << "Error opening file to write." << std::endl;
            return 66;
        }

        buffer << "height,timestamp,ms_diff,blk_diff,hash_rate,is_transition\n"; // column names
        buffer.flush();

        // Mine at least nPeriods level sets
        m.Run();
        int flushedHeight = 0;
        while (flushedHeight < nPeriods) {
            int currentHeight = DAG->GetBestMilestoneHeight();
            if (currentHeight > flushedHeight) {
                const auto& bestChain = DAG->GetBestChain();
                // Write stats
                for (int h = ++flushedHeight; h <= currentHeight; ++h) {
                    auto ms = bestChain.GetStates()[h - bestChain.GetLeastHeightCached()];

                    buffer << ms->height << "," << ms->GetMilestone()->cblock->GetTime() << "," << ms->GetMsDifficulty()
                           << "," << ms->GetBlockDifficulty() << "," << ms->hashRate << ","
                           << (int) ms->IsDiffTransition() << "\n";
                    buffer.flush();
                }
            }
            usleep(100000);
        }

        m.Stop();
        DAG->Stop();
        DAG.reset();
        STORE->Stop();
        STORE.reset();
        WALLET.reset();

        buffer.close();
    }
    return 0;
}
