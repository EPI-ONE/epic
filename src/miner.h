#ifndef __SRC_POW_H__
#define __SRC_POW_H__

#include <atomic>

#include "caterpillar.h"
#include "key.h"
#include "mempool.h"
#include "peer_manager.h"

class Miner {
public:
    Miner() : solverPool_(1) {}
    explicit Miner(size_t nThreads) : solverPool_(nThreads) {}

    bool Start();
    bool Stop();
    void Solve(Block&);
    void Run();

    inline bool IsRunning() {
        return enabled_.load();
    }

    inline ConstBlockPtr GetSelfChainHead() const {
        return selfChainHead;
    }

private:
    std::thread runner_;
    ThreadPool solverPool_;
    std::atomic<bool> enabled_ = false;

    std::atomic<uint32_t> final_nonce;

    ConstBlockPtr selfChainHead = nullptr;
    Cumulator distanceCal;

    uint256 SelectTip();
};

extern std::unique_ptr<Miner> MINER;
#endif /* ifndef __SRC_POW_H__ */
