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

    void Start() {
        bool flag = false;
        if (enabled_.compare_exchange_strong(flag, true)) {
            solverPool_.Start();
            spdlog::info("Miner started.");
        }
    }

    void Stop() {
        bool flag = true;
        if (enabled_.compare_exchange_strong(flag, false)) {
            spdlog::info("Miner stopping...");
            solverPool_.Stop();
            if (runner_.joinable()) {
                runner_.join();
            }
            spdlog::info("Miner stopped.");
        }
    }

    void Solve(Block& b) {
        arith_uint256 target = b.GetTargetAsInteger();
        size_t nthreads      = solverPool_.GetThreadSize();

        std::atomic<uint64_t> final_time = b.GetTime();
        final_nonce                      = 0;

        for (std::size_t i = 1; i <= nthreads; ++i) {
            solverPool_.Execute([&, i]() {
                Block blk(b);
                blk.SetNonce(i);
                blk.FinalizeHash();

                while (final_nonce.load() == 0 && enabled_.load()) {
                    if (UintToArith256(blk.GetHash()) <= target) {
                        final_nonce = blk.GetNonce();
                        final_time  = blk.GetTime();
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
        while (final_nonce.load() == 0 && enabled_.load()) {
            std::this_thread::yield();
        }

        solverPool_.Abort();

        b.SetNonce(final_nonce.load());
        b.SetTime(final_time.load());
        b.CalculateHash();
    }

    void Run() {
        Start();

        runner_ = std::thread([&]() {
            uint256 prevHash;
            Cumulator distanceCal;
            uint32_t counter = 0;

            while (enabled_.load()) {
                Block b(GetParams().version);
                auto head = DAG->GetMilestoneHead();
                assert(head);

                if (!selfChainHead) {
                    prevHash = GENESIS.GetHash();
                    firstRegKey.MakeNewKey(true);
                    b.AddTransaction(Transaction(firstRegKey.GetPubKey().GetID()));
                } else {
                    prevHash = selfChainHead->GetHash();
                    if (distanceCal.Full()) {
                        auto timeInterval = distanceCal.TimeSpan();
                        double percentage =
                            distanceCal.Sum().getdouble() / (timeInterval + 1) / (head->snapshot->hashRate + 1);
                        if (counter % 128 == 0) {
                            std::cout << "Hashing power percentage " << percentage << std::endl;
                        }

                        b.AddTransaction(MEMPOOL->GetTransaction(
                            prevHash,
                            (uint64_t)((GetParams().maxTarget / GetParams().sortitionCoefficient).getdouble() *
                                       percentage)));
                    }
                }

                b.SetMilestoneHash(head->cblock->GetHash());
                b.SetPrevHash(prevHash);
                b.SetTipHash(SelectTip());
                b.SetDifficultyTarget(head->snapshot->blockTarget.GetCompact());

                Solve(b);

                if (!enabled_.load()) {
                    // To prevent continuing with a block having a false nonce,
                    // which may happen when Solve is aborted by calling Miner::Stop
                    return;
                }

                assert(b.CheckPOW());
                b.source = Block::MINER;

                auto bPtr = std::make_shared<const Block>(b);
                peerManager->RelayBlock(bPtr, nullptr);
                distanceCal.Add(bPtr, true);
                selfChainHead = bPtr;
                DAG->AddNewBlock(bPtr, nullptr);
                counter++;
            }
        });
    }

    ConstBlockPtr GetSelfChainHead() const {
        return selfChainHead;
    }

    CKey GetFirstKey() const {
        return firstRegKey;
    }

private:
    std::thread runner_;
    ThreadPool solverPool_;
    std::atomic<bool> enabled_ = false;
    std::atomic<uint32_t> final_nonce;

    ConstBlockPtr selfChainHead = nullptr;

    CKey firstRegKey;

    uint256 SelectTip() {
        ConstBlockPtr selected;
        auto size = DAG->GetBestChain().GetPendingBlockCount();

        for (size_t i = 0; i < size; ++i) {
            selected = DAG->GetBestChain().GetRandomTip();

            if (!selected) {
                break;
            }

            if (selected->source != Block::MINER) {
                return selected->GetHash();
            }
        }

        return GENESIS.GetHash();
    }
};

#endif /* ifndef __SRC_POW_H__ */
