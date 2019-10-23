// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef EPIC_MINER_H
#define EPIC_MINER_H

#include "block_store.h"
#include "circular_queue.h"
#include "key.h"
#include "mempool.h"
#include "peer_manager.h"
#include "solver.h"

template <typename>
class CircularQueue;

class Miner {
public:
    Miner(size_t nThreads = 1);
    ~Miner();

    bool Start();
    bool Stop();
    bool Solve(Block&);
    void Run();

    bool IsRunning() {
        return enabled_.load();
    }

    ConstBlockPtr GetSelfChainHead() const {
        return selfChainHead_;
    }

protected:
    std::atomic_bool enabled_ = false;
    std::atomic_bool abort_   = false;
    std::thread runner_;
    std::thread inspector_;

private:
    Solver* solver;
    ConstBlockPtr selfChainHead_ = nullptr;
    VertexPtr chainHead_         = nullptr;
    Cumulator distanceCal_;
    CircularQueue<uint256> selfChainHeads_;

    uint256 SelectTip();
};

extern std::unique_ptr<Miner> MINER;

#endif // EPIC_MINER_H
