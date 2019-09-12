// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "miner.h"

void SetNonce(VStream& vs, uint32_t nonce) {
    memcpy(vs.data() + vs.size() - sizeof(uint32_t), &nonce, sizeof(uint32_t));
}

void SetTimestamp(VStream& vs, uint32_t t) {
    memcpy(vs.data() + vs.size() - 3 * sizeof(uint32_t), &t, sizeof(uint32_t));
}

Miner::Miner() : solverPool_(1) {}

Miner::Miner(size_t nThreads, size_t nSipThreads) {
#ifndef __CUDA_ENABLED__
    spdlog::info("No CUDA support. Miner aborted.");
#else
    FillDefaultGPUParams(solverParams);
    int nGPUDevices{};
    checkCudaErrors_V(cudaGetDeviceCount(&nGPUDevices));
    spdlog::info("Miner using GPU. Found {} GPU devices.", nGPUDevices);

    nThreads = std::min(nThreads, (size_t) nGPUDevices);
#endif

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
    b.CalculateOptimalEncodingSize();
}

void Miner::SolveCuckaroo(Block& b) {
#ifdef __CUDA_ENABLED__
    arith_uint256 target = b.GetTargetAsInteger();
    size_t nthreads      = solverPool_.GetThreadSize();

    final_nonce     = 0;
    final_ctx_index = -1;
    final_time      = b.GetTime();
    VStream vs(b.GetHeader());

    std::vector<std::unique_ptr<SolverCtx>> ctx_q(nthreads);

    for (size_t i = 0; i < nthreads; ++i) {
        solverPool_.Execute([&, i]() {
            auto blkStream     = vs;
            uint32_t nonce     = i;
            uint32_t timestamp = time(nullptr);
            SetNonce(blkStream, nonce);
            SetTimestamp(blkStream, time(nullptr));

            auto _params    = solverParams;
            _params.device  = i;
            ctx_q[i]        = std::unique_ptr<SolverCtx>(CreateSolverCtx(_params));
            const auto& ctx = ctx_q[i];

            while (final_nonce.load() == 0 && enabled_.load()) {
                if (nonce % 128 == 0) {
                    std::cout << "Solving for nonce " << nonce << std::endl;
                }

                ctx->SetHeader(blkStream.data(), blkStream.size());

                if (ctx->solve()) {
                    uint256 cyclehash = HashBLAKE2<256>(&ctx->sols[ctx->sols.size() - CYCLELEN], PROOFSIZE);
                    spdlog::trace("Found solution with nonce {}: {} v.s. target {}.", nonce, cyclehash.to_substr(),
                                  ArithToUint256(target).to_substr());
                    if (UintToArith256(cyclehash) <= target) {
                        size_t minus_one = -1;
                        if (final_ctx_index.compare_exchange_strong(minus_one, i)) {
                            final_nonce = nonce;
                            final_time  = timestamp;
                        }
                        break;
                    }
                }

                if (nonce >= UINT_LEAST32_MAX - nthreads) {
                    nonce = i;
                    SetNonce(blkStream, nonce);
                    SetTimestamp(blkStream, time(nullptr));
                } else {
                    nonce += nthreads;
                    memcpy(blkStream.data() + blkStream.size() - sizeof(uint32_t), &nonce, sizeof(uint32_t));
                }
            }
        });
    }

    while (final_nonce.load() == 0 && enabled_.load()) {
        usleep(500000);
    }

    // Abort unfinished tasks
    solverPool_.ClearAndDisableTasks();
    for (const auto& ctx : ctx_q) {
        ctx->abort();
    }
    solverPool_.Abort();

    spdlog::trace("Final nonce {}", final_nonce.load());
    b.SetNonce(final_nonce.load());
    auto& final_sols = ctx_q[final_ctx_index]->sols;
    b.SetProof(final_sols.data() + final_sols.size() - GetParams().cycleLen);
    b.SetTime(final_time.load());
    b.CalculateHash();
    b.CalculateOptimalEncodingSize();
    assert(b.CheckPOW());
#endif
}

void Miner::Run() {
#ifndef __CUDA_ENABLED__
    spdlog::info("No CUDA support. Abort.");
#else
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
                    auto timeInterval = distanceCal.TimeSpan();
                    double percentage =
                        distanceCal.Sum().GetDouble() / (timeInterval + 1) / (head->snapshot->hashRate + 1);
                    if (counter % 2000 == 0) {
                        std::cout << "Hashing power percentage " << percentage << std::endl;
                    }

                    auto tx = MEMPOOL->GetRedemptionTx(false);
                    if (tx) {
                        b.AddTransaction(std::move(tx));
                    }

                    auto allowed = (distanceCal.Sum() / (distanceCal.TimeSpan() + 1)) /
                                   GetParams().sortitionCoefficient *
                                   (GetParams().maxTarget / (head->snapshot->hashRate + 1));
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
#endif
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
