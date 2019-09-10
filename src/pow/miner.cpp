// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "miner.h"

Miner::Miner() : Miner(4) {}

Miner::Miner(size_t nThreads) {
    if (CYCLELEN) {
#ifndef __CUDA_ENABLED__
        spdlog::info("No CUDA support.");
#else
        FillDefaultGPUParams(solverParams);
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
            auto _params   = solverParams;
            _params.device = i;
            SolverCtx* ctx = nullptr;
            while (!found_sols.load() && !ctx) {
                ctx = CreateSolverCtx(_params);
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

                if (found_sols.load()) {
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
                        break;
                    }
                }
            }
        });
    }

    while (!found_sols.load() && enabled_.load()) {
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

    for (auto& ctx : ctx_q) {
        if (ctx) {
            DestroySolverCtx(ctx);
        }
    }
#endif
}

void Miner::Run() {
#ifndef __CUDA_ENABLED__
    spdlog::info("No CUDA support.");
#endif
    if (Start()) {
        // Restore miner chain head
        auto headHash = STORE->GetMinerChainHead();
        if (!headHash.IsNull()) {
            selfChainHead = STORE->FindBlock(headHash);
        }

        // Restore distanceCal
        if (selfChainHead && distanceCal.Empty()) {
            auto cursor = selfChainHead;
            do {
                distanceCal.Add(cursor, false);
                cursor = STORE->FindBlock(cursor->GetPrevHash());
            } while (*selfChainHead != GENESIS && !distanceCal.Full());
        }
    }

    runner_ = std::thread([&]() {
        uint256 prevHash;
        uint32_t counter = 0;
        uint32_t ms_cnt  = 0;

        while (enabled_.load()) {
            Block b(GetParams().version);
            auto head = DAG->GetMilestoneHead();

            if (!head) {
                spdlog::error("Cannot get milestone head. Did you init with new DB (with flag \"-N\")?");
                enabled_ = false;
                spdlog::info("Miner stopped.");
                return;
            }

            if (!selfChainHead) {
                auto firstRegTx = MEMPOOL->GetRedemptionTx(true);
                if (!firstRegTx) {
                    spdlog::warn("Can't get the first registration tx, keep waiting...");
                    while (!firstRegTx) {
                        std::this_thread::yield();
                        firstRegTx = MEMPOOL->GetRedemptionTx(true);
                    }
                }
                spdlog::info("Got the first registration. Start mining.");
                prevHash = GENESIS.GetHash();
                b.AddTransaction(std::move(firstRegTx));
            } else {
                prevHash = selfChainHead->GetHash();
                if (distanceCal.Full()) {
                    if (counter % 10 == 0) {
                        spdlog::debug("Hashing power percentage {}", distanceCal.Sum().GetDouble() /
                                                                         (distanceCal.TimeSpan() + 1) /
                                                                         (head->snapshot->hashRate + 1));
                    }

                    auto tx = MEMPOOL->GetRedemptionTx(false);
                    if (tx) {
                        b.AddTransaction(std::move(tx));
                    }

                    auto allowed = CalculateAllowedDist(distanceCal, head->snapshot->hashRate);
                    b.AddTransactions(MEMPOOL->ExtractTransactions(prevHash, allowed, GetParams().blockCapacity));
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
            if (PEERMAN) {
                PEERMAN->RelayBlock(bPtr, nullptr);
            }
            distanceCal.Add(bPtr, true);
            selfChainHead = bPtr;
            DAG->AddNewBlock(bPtr, nullptr);
            STORE->SaveMinerChainHead(bPtr->GetHash());

            if (CheckMsPOW(bPtr, head->snapshot)) {
                spdlog::debug("🚀 Mined a milestone {}", bPtr->GetHash().to_substr());
                ms_cnt++;
                // Block the thread until the verification is done
                while (*DAG->GetMilestoneHead()->cblock == *head->cblock) {
                    std::this_thread::yield();
                }
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

    return GENESIS.GetHash();
}
