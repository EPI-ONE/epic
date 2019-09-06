// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef EPIC_MINER_H
#define EPIC_MINER_H

#include "key.h"
#include "mempool.h"
#include "peer_manager.h"
#include "storage.h"
#include "trimmer.h"

#include <atomic>

class Miner {
public:
    Miner() : solverPool_(1) {}
    explicit Miner(size_t nThreads, size_t nSipThreads = 0) {
#ifndef __CUDA_ENABLED__
        if ((nSipThreads & (nSipThreads - 1)) != 0) { // make sure it's power of 2
            // Round it to the largest power of 2 less than nSipThreads
            uint32_t n = 1 << (sizeof(uint32_t) * 8 - 1);
            while ((nSipThreads & n) == 0 && n != 0) {
                n >>= 1;
            }
            nSipThreads = n;
        }

        params.nthreads = nSipThreads;
        params.ntrims   = EDGEBITS >= 30 ? 96 : 68;
        spdlog::info("Miner using CPU. {} threads in solver pool.", nThreads);
#else
        FillDefaultGPUParams(&params);
        int nGPUDevices{};
        checkCudaErrors_V(cudaGetDeviceCount(&nGPUDevices));
        spdlog::info("Miner using GPU. Found {} GPU devices.", nGPUDevices);

        nThreads = std::min(nThreads, (size_t) nGPUDevices);
#endif

        solverPool_.SetThreadSize(nThreads);
    }

    bool Start();
    bool Stop();
    void Solve(Block&);
    void SolveCuckaroo(Block&);
    void Run();

    bool IsRunning() {
        return enabled_.load();
    }

    ConstBlockPtr GetSelfChainHead() const {
        return selfChainHead;
    }

private:
    std::thread runner_;
    ThreadPool solverPool_;
    std::atomic<bool> enabled_ = false;

    std::atomic<uint32_t> final_nonce;
    std::atomic<uint64_t> final_time;

    ConstBlockPtr selfChainHead = nullptr;
    Cumulator distanceCal;

    SolverParams params;

    uint256 SelectTip();
};

extern std::unique_ptr<Miner> MINER;
#endif // EPIC_MINER_H
