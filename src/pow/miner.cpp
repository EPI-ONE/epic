// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "miner.h"
#include "wallet.h"

const size_t headsCacheLimit = 20;

Miner::Miner(size_t nThreads) : selfChainHeads_(headsCacheLimit) {
    if (CYCLELEN) {
        solver = new RemoteGPUSolver();
    } else {
        solver = new CPUSolver(nThreads);
    }
}

Miner::~Miner() {
    delete solver;
}

bool Miner::Start() {
    if (solver->Start()) {
        enabled_ = true;
        spdlog::info("Miner started.");
        return true;
    }

    enabled_ = false;
    return false;
}

bool Miner::Stop() {
    spdlog::info("Stopping miner...");

    enabled_ = false;

    if (runner_.joinable()) {
        runner_.join();
    }
    if (inspector_.joinable()) {
        inspector_.join();
    }
    if (solver->Stop()) {
        return true;
    }

    return false;
}

bool Miner::Solve(Block& b) {
    return solver->Solve(b);
}

void Miner::Run() {
    if (!Start()) {
        return;
    }

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

    chainHead_ = DAG->GetMilestoneHead();

    inspector_ = std::thread([&]() {
        while (enabled_.load()) {
            auto headInDAG = DAG->GetMilestoneHead()->cblock;
            if (!abort_ && headInDAG->source == Block::NETWORK &&
                *headInDAG != *std::atomic_load(&chainHead_)->cblock) {
                abort_ = true;
                solver->Abort();
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
            solver->Resume();
            Block b(GetParams().version);

            if (!selfChainHead_) {
                if (WALLET) {
                    WALLET->EnableFirstReg();
                }

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
                prevHash     = selfChainHead_->GetHash();
                auto max_ntx = GetParams().blockCapacity;

                auto tx = MEMPOOL->GetRedemptionTx(false);
                if (tx && !tx->IsFirstRegistration()) {
                    b.AddTransaction(std::move(tx));
                    max_ntx--;
                }

                if (distanceCal_.Full()) {
                    if (counter % 10 == 0) {
                        spdlog::info("Hashing power percentage {}",
                                     distanceCal_.Sum().GetDouble() / std::max(distanceCal_.TimeSpan(), (uint32_t) 1) /
                                         std::atomic_load(&chainHead_)->snapshot->hashRate);
                    }

                    auto allowed =
                        CalculateAllowedDist(distanceCal_, std::atomic_load(&chainHead_)->snapshot->hashRate);
                    b.AddTransactions(MEMPOOL->ExtractTransactions(prevHash, allowed, max_ntx));
                }
            }

            b.SetMerkle();
            b.SetMilestoneHash(std::atomic_load(&chainHead_)->cblock->GetHash());
            b.SetPrevHash(prevHash);
            b.SetTipHash(SelectTip());
            b.SetDifficultyTarget(std::atomic_load(&chainHead_)->snapshot->blockTarget.GetCompact());

            if (!Solve(b)) {
                spdlog::warn("Failed to solve the block. Stop the miner.");
                enabled_ = false;
                solver->Stop();
                if (inspector_.joinable()) {
                    inspector_.join();
                }
            }

            // To prevent continuing with a block having a false nonce,
            // which may happen when Solve is aborted by calling
            // Miner::Stop or when abort_ = true
            if (abort_.load() || !enabled_.load()) {
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
