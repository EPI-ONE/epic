// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "miner.h"
#include "wallet.h"

const size_t headsCacheLimit = 20;

Miner::Miner(size_t nThreads) : selfChainHeads_(headsCacheLimit) {
    if (GetParams().cycleLen) {
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
        DAG->RegisterOnChainUpdatedCallback(
            std::bind(&Miner::OnDAGHeadUpdated, this, std::placeholders::_1, std::placeholders::_2));
        spdlog::info("Miner started.");
        return true;
    }

    enabled_ = false;
    return false;
}

bool Miner::Stop() {
    spdlog::info("Stopping miner...");

    DAG->RegisterOnChainUpdatedCallback(nullptr);
    enabled_ = false;
    continue_.notify_all();

    if (runner_.joinable()) {
        runner_.join();
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

    runner_ = std::thread([&]() {
        uint256 prevHash;
        uint32_t counter = 0;
        uint32_t ms_cnt  = 0;

        while (enabled_.load()) {
            auto ms_head = DAG->GetMilestoneHead();
            Block b(GetParams().version);

            if (!selfChainHead_) {
                spdlog::info("[Miner] Paused. Waiting for the first registration...");
                ConstTxPtr firstRegTx;
                while (enabled_ && !(firstRegTx = MEMPOOL->GetFirstReg()))
                    ;
                spdlog::info("[Miner] Got the first registration. Start mining.");

                prevHash = GENESIS->GetHash();
                b.AddTransaction(std::move(firstRegTx));
            } else {
                prevHash     = selfChainHead_->GetHash();
                auto max_ntx = GetParams().blockCapacity;

                auto tx = MEMPOOL->GetRedemptionTx();

                if (tx) {
                    if (tx->IsFirstRegistration()) {
                        // Start with a new peer chain
                        selfChainHead_ = nullptr;
                        prevHash       = GENESIS->GetHash();
                        selfChainHeads_.clear();
                        distanceCal_.Clear();
                    } else {
                        b.AddTransaction(std::move(tx));
                        max_ntx--;
                    }
                }

                if (distanceCal_.Full()) {
                    if (counter % 10 == 0) {
                        spdlog::debug("[Miner] Hashing power percentage {}",
                                      distanceCal_.Sum().GetDouble() / std::max(distanceCal_.TimeSpan(), (uint32_t) 1) /
                                          ms_head->snapshot->hashRate);
                    }

                    auto allowed = CalculateAllowedDist(distanceCal_, ms_head->snapshot->hashRate);
                    b.AddTransactions(MEMPOOL->ExtractTransactions(prevHash, allowed, max_ntx));
                }
            }

            b.SetMerkle();
            b.SetMilestoneHash(ms_head->cblock->GetHash());
            b.SetPrevHash(prevHash);
            b.SetTipHash(SelectTip());
            b.SetDifficultyTarget(ms_head->snapshot->blockTarget.GetCompact());

            int ret = Solve(b);

            // To prevent continuing with a block having a false nonce,
            // which may happen when Solve is aborted by calling
            // Miner::Stop or when abort_ = true
            if (ret == SolverResult::ErrorCode::SERVER_ABORT || !b.Verify()) {
                if (b.HasTransaction()) {
                    auto txns         = b.GetTransactions();
                    size_t startIndex = 0;
                    if (b.IsRegistration()) {
                        if (b.IsFirstRegistration() || txns[0]->GetOutputs()[0].value.GetValue()) {
                            MEMPOOL->PushRedemptionTx(std::move(txns[0]));
                        }
                        startIndex = 1;
                    }
                    for (size_t i = startIndex; i < b.GetTransactionSize(); ++i) {
                        MEMPOOL->Insert(std::move(txns[i]));
                    }
                }
                continue;
            } else if (ret == SolverResult::ErrorCode::SUCCESS) {
                b.source = Block::MINER;

                auto bPtr = std::make_shared<const Block>(b);

                if (PEERMAN) {
                    PEERMAN->RelayBlock(bPtr, nullptr);
                }

                distanceCal_.Add(bPtr, true);
                selfChainHead_ = bPtr;
                selfChainHeads_.push(bPtr->GetHash());
                dag_updated_ = false;
                DAG->AddNewBlock(bPtr, nullptr);
                STORE->SaveMinerChainHeads(selfChainHeads_);

                if (CheckMsPOW(bPtr, ms_head->snapshot)) {
                    spdlog::info("🚀 Mined a milestone {}, ms {} prev {} tip {} blockTarget {}",
                                 bPtr->GetHash().to_substr(), bPtr->GetMilestoneHash().to_substr(),
                                 bPtr->GetPrevHash().to_substr(), bPtr->GetTipHash().to_substr(),
                                 bPtr->GetDifficultyTarget());
                    ms_cnt++;
                    // Block the thread until the verification is done
                    WaitDAGHeadUpdate();
                }
                counter++;

            } else {
                spdlog::warn("[Miner] Failed to solve the block. Stop the miner.");
                enabled_ = false;
                solver->Stop();
            }
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

void Miner::WaitDAGHeadUpdate() {
    std::unique_lock<std::mutex> lock(mtx_);
    continue_.wait(lock, [this] { return dag_updated_ || !enabled_; });
}

void Miner::OnDAGHeadUpdated(ConstBlockPtr chain_ms_head, bool isMainchain) {
    std::unique_lock<std::mutex> lock(mtx_);
    if (isMainchain && chain_ms_head->source != Block::MINER) {
        solver->Abort();
        spdlog::debug("[Miner] Milestone chain head changed {}. Abort the current task.",
                      chain_ms_head->GetHash().to_substr());
    }

    if (isMainchain || chain_ms_head->source == Block::MINER) {
        dag_updated_ = true;
        continue_.notify_all();
    }
}
