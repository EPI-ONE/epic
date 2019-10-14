// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "miner.h"

const size_t headsCacheLimit = 20;

Miner::Miner() : Miner(1) {}

Miner::Miner(size_t nThreads) : selfChainHeads_(headsCacheLimit) {
    if (CYCLELEN) {
#ifndef __CUDA_ENABLED__
        spdlog::info("No CUDA support.");
#else
        FillDefaultGPUParams(solverParams_);
        int nGPUDevices{};
        gpuAssert(cudaGetDeviceCount(&nGPUDevices), (char*) __FILE__, __LINE__);
        spdlog::info("Miner using GPU. Found {} GPU devices.", nGPUDevices);

        nThreads = std::max(nThreads, (size_t) 1);
        nThreads = std::min(nThreads, (size_t) nGPUDevices);
#endif
    }

    solverPool_.SetThreadSize(nThreads);
}

bool Miner::Start() {
    bool flag = false;
    if (enabled_.compare_exchange_strong(flag, true)) {
        solverPool_.Start();
        spdlog::info("Miner started.");
        return true;
    }

    return false;
}

bool Miner::Stop() {
    bool flag = true;
    if (enabled_.compare_exchange_strong(flag, false)) {
        spdlog::info("Miner stopping...");
        solverPool_.Stop();
        if (runner_.joinable()) {
            runner_.join();
        }
        if (inspector_.joinable()) {
            inspector_.join();
        }
        spdlog::info("Miner stopped.");
        return true;
    }

    return false;
}

void Miner::Solve(Block& b) {
#ifdef __CUDA_ENABLED__
    arith_uint256 target = b.GetTargetAsInteger();
    size_t nthreads      = solverPool_.GetThreadSize();

    final_nonce = 0;
    final_time  = b.GetTime();
    found_sols  = false;
    std::vector<uint32_t> final_sol(CYCLELEN);

    std::vector<SolverCtx*> ctx_q(nthreads);
    VStream vs(b.GetHeader());

    for (size_t i = 0; i < nthreads; ++i) {
        solverPool_.Execute([&, i]() {
            auto params    = solverParams_;
            params.device  = i;
            SolverCtx* ctx = nullptr;
            while (!found_sols.load() && !ctx) {
                ctx = CreateSolverCtx(params);
            }
            ctx_q[i] = ctx;

            auto blkStream     = vs;
            uint32_t nonce     = i - nthreads;
            uint32_t timestamp = final_time.load();

            while (enabled_.load()) {
                nonce += nthreads;
                SetNonce(blkStream, nonce);

                if (nonce >= i - nthreads) {
                    timestamp = time(nullptr);
                    SetTimestamp(blkStream, timestamp);
                }

                ctx->SetHeader(blkStream.data(), blkStream.size());

                if (found_sols.load() || abort_.load()) {
                    delete ctx;
                    return;
                }

                if (ctx->solve()) {
                    std::vector<uint32_t> sol(ctx->sols.end() - CYCLELEN, ctx->sols.end()); // the last solution
                    uint256 cyclehash = HashBLAKE2<256>(sol.data(), PROOFSIZE);
                    if (UintToArith256(cyclehash) <= target) {
                        bool f = false;
                        if (found_sols.compare_exchange_strong(f, true)) {
                            final_nonce = nonce;
                            final_time  = timestamp;
                            final_sol   = std::move(sol);
                            spdlog::trace("Found solution: thread {}, nonce {}, time {}, cycle hash {}", i, nonce,
                                          timestamp, cyclehash.to_substr());
                        }
                        delete ctx;
                        break;
                    }
                }
            }
        });
    }

    while (!found_sols.load() && enabled_.load() && !abort_.load()) {
        usleep(500000);
    }

    // Abort unfinished tasks
    solverPool_.ClearAndDisableTasks();
    for (const auto& ctx : ctx_q) {
        if (ctx) {
            ctx->abort();
        }
    }
    solverPool_.Abort();

    b.SetNonce(final_nonce.load());
    b.SetProof(std::move(final_sol));
    b.SetTime(final_time.load());
    b.CalculateHash();
    b.CalculateOptimalEncodingSize();
#endif
}

void Miner::Run() {
#ifndef __CUDA_ENABLED__
    spdlog::info("No CUDA support.");
#endif
    if (Start()) {
        // Restore miner chain head
        selfChainHeads_ = STORE->GetMinerChainHeads();
        selfChainHeads_.set_limit(headsCacheLimit);
        while (!selfChainHeads_.empty() && !(selfChainHead_ = STORE->FindBlock(selfChainHeads_.front()))) {
            selfChainHeads_.pop_front();
        }

        // Restore distanceCal_
        if (selfChainHead_ && distanceCal_.Empty()) {
            auto cursor = selfChainHead_;
            do {
                distanceCal_.Add(cursor, false);
                cursor = STORE->FindBlock(cursor->GetPrevHash());
            } while (*cursor != *GENESIS && !distanceCal_.Full());
        }
    }

    chainHead_ = DAG->GetMilestoneHead();

    inspector_ = std::thread([&]() {
        while (enabled_.load()) {
            auto headInDAG = DAG->GetMilestoneHead()->cblock;
            if (!abort_ && headInDAG->source == Block::NETWORK &&
                *headInDAG != *std::atomic_load(&chainHead_)->cblock) {
                abort_ = true;
                spdlog::debug("Milestone chain head changed {} => {}. Abort the current task.",
                              std::atomic_load(&chainHead_)->cblock->GetHash().to_substr(),
                              headInDAG->GetHash().to_substr());
                std::atomic_store(&chainHead_, DAG->GetMilestoneHead());
            } else {
                usleep(10000);
            }
        }
    });

    runner_ = std::thread([&]() {
        uint256 prevHash;
        uint32_t counter = 0;
        uint32_t ms_cnt  = 0;

        while (enabled_.load()) {
            abort_ = false;
            Block b(GetParams().version);

            if (!selfChainHead_) {
                auto firstRegTx = MEMPOOL->GetRedemptionTx(true);
                if (!firstRegTx) {
                    spdlog::warn("Paused. Waiting for the first registration.");
                    while (!firstRegTx) {
                        std::this_thread::yield();
                        firstRegTx = MEMPOOL->GetRedemptionTx(true);
                    }
                }
                spdlog::info("Got the first registration. Start mining.");
                prevHash = GENESIS->GetHash();
                b.AddTransaction(std::move(firstRegTx));
            } else {
                prevHash = selfChainHead_->GetHash();

                auto tx = MEMPOOL->GetRedemptionTx(false);
                if (tx && !tx->IsFirstRegistration()) {
                    b.AddTransaction(std::move(tx));
                }

                if (distanceCal_.Full()) {
                    if (counter % 10 == 0) {
                        spdlog::info("Hashing power percentage {}",
                                     distanceCal_.Sum().GetDouble() / (distanceCal_.TimeSpan() + 1) /
                                         (double) (std::atomic_load(&chainHead_)->snapshot->hashRate + 1));
                    }

                    auto allowed =
                        CalculateAllowedDist(distanceCal_, std::atomic_load(&chainHead_)->snapshot->hashRate);
                    b.AddTransactions(MEMPOOL->ExtractTransactions(prevHash, allowed, GetParams().blockCapacity));
                }
            }

            b.SetMerkle();
            b.SetMilestoneHash(std::atomic_load(&chainHead_)->cblock->GetHash());
            b.SetPrevHash(prevHash);
            b.SetTipHash(SelectTip());
            b.SetDifficultyTarget(std::atomic_load(&chainHead_)->snapshot->blockTarget.GetCompact());

            Solve(b);

            // To prevent continuing with a block having a false nonce,
            // which may happen when Solve is aborted by calling
            // Miner::Stop or when abort_ = true
            if (!enabled_.load()) {
                return;
            }
            if (abort_.load()) {
                if (b.HasTransaction()) {
                    auto txns         = b.GetTransactions();
                    size_t startIndex = 0;
                    if (txns[0]->IsRegistration()) {
                        MEMPOOL->PushRedemptionTx(std::move(txns[0]));
                        startIndex = 1;
                    }
                    for (size_t i = startIndex; i < b.GetTransactionSize(); ++i) {
                        MEMPOOL->Insert(std::move(txns[i]));
                    }
                }
                continue;
            }

            assert(b.CheckPOW());
            b.source = Block::MINER;

            auto bPtr = std::make_shared<const Block>(b);

            if (PEERMAN) {
                PEERMAN->RelayBlock(bPtr, nullptr);
            }

            distanceCal_.Add(bPtr, true);
            selfChainHead_ = bPtr;
            selfChainHeads_.push(bPtr->GetHash());
            DAG->AddNewBlock(bPtr, nullptr);
            STORE->SaveMinerChainHeads(selfChainHeads_);

            if (CheckMsPOW(bPtr, std::atomic_load(&chainHead_)->snapshot)) {
                spdlog::info("🚀 Mined a milestone {}", bPtr->GetHash().to_substr());
                ms_cnt++;
                // Block the thread until the verification is done
                while (*DAG->GetMilestoneHead()->cblock == *std::atomic_load(&chainHead_)->cblock) {
                    std::this_thread::yield();
                }

                std::atomic_store(&chainHead_, DAG->GetMilestoneHead());
            }

            counter++;
        }
    });
}

uint256 Miner::SelectTip() {
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

    return GENESIS->GetHash();
}
