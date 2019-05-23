#ifndef __SRC_POW_H__
#define __SRC_POW_H__

#include <atomic>

#include "block.h"
#include "threadpool.h"

void Solve(Block b, ThreadPool& solverPool) {
    arith_uint256 target = b.GetTargetAsInteger();
    std::size_t nthreads = solverPool.size();
    uint64_t final_time  = b.GetTime();

    uint32_t default_nonce            = 0;
    std::atomic<uint32_t> final_nonce = default_nonce;

    for (std::size_t i = 1; i <= nthreads; ++i) {
        solverPool.Execute([=, &final_nonce, &final_time, &default_nonce]() {
            Block blk(b);
            blk.SetNonce(i);
            blk.FinalizeHash();

            while (final_nonce.load() == 0) {
                if (UintToArith256(blk.GetHash()) <= target) {
                    if (final_nonce.compare_exchange_strong(default_nonce, blk.GetNonce())) {
                        final_time = blk.GetTime();
                    }

                    break;
                }

                if (blk.GetNonce() >= UINT_LEAST32_MAX - nthreads) {
                    blk.SetTime(time(nullptr));
                    blk.SetNonce(i);
                } else {
                    blk.SetNonce(blk.GetNonce() + nthreads);
                }

                blk.CalculateHash();
            }
        });
    }

    // Block the main thread until a nonce is solved
    while (final_nonce.load() == 0) {
        std::this_thread::yield();
    }

    b.SetNonce(final_nonce.load());
    b.SetTime(final_time);
    b.FinalizeHash();
}

#endif /* ifndef __SRC_POW_H__ */
