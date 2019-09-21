// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "miner.h"
#include "solver_protocol.h"
const size_t headsCacheLimit = 20;

Miner::Miner() : selfChainHeads_(headsCacheLimit) {}

bool Miner::Start() {
    bool flag = false;
    if (enabled_.compare_exchange_strong(flag, true)) {
        auto client_ip = NetAddress::GetByIP(CONFIG->GetSolverAddr());
        if (client_ip) {
            client = std::make_unique<MinerRPCClient>(client_ip->ToString());
        } else {
            spdlog::info("Invalid solver RPC client ip. Failed to start the miner.");
            return false;
        }
        spdlog::info("Miner started.");
        return true;
    }
    return false;
}

bool Miner::Stop() {
    bool flag = true;
    if (enabled_.compare_exchange_strong(flag, false)) {
        spdlog::info("Stopping miner...");
        if (runner_.joinable()) {
            runner_.join();
        }
        if (inspector_.joinable()) {
            inspector_.join();
        }
        return true;
    }

    return false;
}

bool Miner::Solve(Block& b) {
    arith_uint256 target = b.GetTargetAsInteger();
    VStream vs(b.GetHeader());
    grpc::ClientContext context;
    POWTask request;
    POWResult reply;
    request.set_task_id(rand());
    request.set_init_nonce(0);
    request.set_init_time(b.GetTime());
    request.set_step(1);
    request.set_cycle_length(CYCLELEN);
    request.set_target(target.GetHex());
    request.set_header(vs.data(), vs.size());

    current_task_id_ = request.task_id();
    if (!sent_task_) {
        sent_task_ = true;
    }
    std::cout << "Sending solver task: id = " << request.task_id() << std::endl;
    auto status = client->SendTask(&context, request, &reply);

    bool flag = false;
    if (status.ok()) {
        switch (reply.error_code()) {
            case SolverResult::ErrorCode ::SUCCESS: {
                b.SetNonce(reply.nonce());
                std::vector<uint32_t> proof;
                for (auto n : reply.proof()) {
                    proof.emplace_back(n);
                }
                b.SetProof(std::move(proof));
                b.SetTime(reply.time());
                b.CalculateHash();
                b.CalculateOptimalEncodingSize();
                spdlog::info("Solver task succeeded, id = {}", request.task_id());
                flag = true;
                break;
            }
            case SolverResult::ErrorCode::SERVER_ABORT: {
                spdlog::warn("Remote solver aborted. Task failed: id = {}", request.task_id());
                break;
            }
            case SolverResult::ErrorCode ::TASK_CANCELED_BY_CLIENT: {
                spdlog::warn("We canceled this task: id = {}", request.task_id());
                flag = true;
                break;
            }
            case SolverResult::ErrorCode::INVALID_PARAM: {
                spdlog::warn("Invalid task parameter. Task failed: id = {}", request.task_id());
                break;
            }
            case SolverResult::ErrorCode::UNKNOWN_ERROR: {
                spdlog::warn("Unknown error on remote solver. Task failed: id = {}", request.task_id());
                break;
            }
            default: {
                spdlog::warn("Unknown error code. Task failed: id = {}", request.task_id());
                break;
            }
        }
    } else {
        spdlog::warn("RPC error. Task failed: id = {}, error message = ", request.task_id(), status.error_message());
    }
    return flag;
}

void Miner::Run() {
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
                AbortTask(current_task_id_);
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

            if (!Solve(b)) {
                spdlog::warn("Failed to solve the block. Stop the miner.");
                return;
            }

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
void Miner::AbortTask(uint32_t task_id) {
    if (!sent_task_) {
        return;
    }
    grpc::ClientContext context;
    StopTaskRequest request;
    StopTaskResponse response;
    request.set_task_id(task_id);
    client->AbortTask(&context, request, &response);
}
