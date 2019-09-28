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
#include "trimmer.h"

#include <atomic>

template <typename>
class CircularQueue;

class Miner {
public:
    Miner();
    Miner(size_t nThreads);
    virtual ~Miner() {}

    bool Start();
    bool Stop();
    virtual void Solve(Block&);
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

    ThreadPool solverPool_;
    std::atomic<uint32_t> final_nonce;
    std::atomic<uint64_t> final_time;
    std::atomic<bool> found_sols;

private:
    std::thread runner_;
    std::thread inspector_;

    SolverParams solverParams_;

    ConstBlockPtr selfChainHead_ = nullptr;
    VertexPtr chainHead_         = nullptr;
    Cumulator distanceCal_;
    CircularQueue<uint256> selfChainHeads_;

    uint256 SelectTip();
};

extern std::unique_ptr<Miner> MINER;

inline void SetNonce(VStream& vs, uint32_t nonce) {
    memcpy(vs.data() + vs.size() - sizeof(uint32_t), &nonce, sizeof(uint32_t));
}

inline void SetTimestamp(VStream& vs, uint32_t t) {
    memcpy(vs.data() + vs.size() - 3 * sizeof(uint32_t), &t, sizeof(uint32_t));
}

#endif // EPIC_MINER_H
