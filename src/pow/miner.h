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
    Miner();
    Miner(size_t nThreads, size_t nSipThreads = 0);

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

    SolverParams solverParams;
    std::atomic<uint32_t> final_nonce;
    std::atomic<uint64_t> final_time;
    std::atomic<size_t> final_ctx_index;

    ConstBlockPtr selfChainHead = nullptr;
    Cumulator distanceCal;

    SolverParams params;

    uint256 SelectTip();
};

extern std::unique_ptr<Miner> MINER;
#endif // EPIC_MINER_H
