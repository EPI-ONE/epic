// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "solver_manager.h"
#include "hash.h"
#include "service/solver.h"

inline void SetNonce(VStream& vs, uint32_t nonce) {
    memcpy(vs.data() + vs.size() - sizeof(uint32_t), &nonce, sizeof(uint32_t));
}

inline void SetTimestamp(VStream& vs, uint32_t t) {
    memcpy(vs.data() + vs.size() - 3 * sizeof(uint32_t), &t, sizeof(uint32_t));
}

SolverManager::SolverManager(size_t nThreads) : solverPool_(nThreads) {
    FillDefaultGPUParams(solverParams_);
}

bool SolverManager::Start() {
    bool flag = false;
    if (enabled_.compare_exchange_strong(flag, true)) {
        solverPool_.Start();
        spdlog::info("Solver started.");
        return true;
    }

    return false;
}

bool SolverManager::Stop() {
    bool flag = true;
    if (enabled_.compare_exchange_strong(flag, false)) {
        spdlog::info("Stopping solver...");
        solverPool_.Stop();

        return true;
    }
    return false;
}

TaskStatus SolverManager::ProcessTask(const std::shared_ptr<SolverTask>& task) {
    task_queue_.Put(task->id);
    tasks.insert({task->id, task});
    spdlog::info("Received task with id = {}", task->id);
    while (!isIdle || task_queue_.Front() != task->id) {
        sleep(1);
    }

    isIdle = false;
    TaskStatus result;
    task_queue_.Take(task->id);
    if (task->abort_) {
        result.second = SolverResult::ErrorCode ::TASK_CANCELED_BY_CLIENT;
        spdlog::info("Aborting task with id = {}", task->id);
    } else if (!enabled_) {
        result.second = SolverResult::ErrorCode ::SERVER_ABORT;
        spdlog::info("Server shutted down. Aborting task with id = {}", task->id);
    } else {
        result = Solve(task);
        spdlog::info("Finished task with id = {}", task->id);
    }
    tasks.erase(task->id);
    isIdle = true;
    return result;
}


TaskStatus SolverManager::Solve(std::shared_ptr<SolverTask> task) {
    aborted_ = false;
    solutions.Enable();
    TaskStatus status;

    size_t nthreads = solverPool_.GetThreadSize();
    std::vector<SolverCtx*> ctx_q(nthreads);

    try {
        for (size_t i = 0; i < nthreads; ++i) {
            solverPool_.Execute([i, nthreads, nonce = task->init_nonce + i * task->step, timestamp = task->init_time,
                                 &task, &ctx_q, this]() mutable {
                auto params    = solverParams_;
                params.device  = i;
                SolverCtx* ctx = nullptr;
                while (!aborted_ && !task->abort_ && !ctx) {
                    ctx = CreateSolverCtx(params, task->cycle_length);
                }
                ctx_q[i] = ctx;

                auto blkStream = task->blockHeader;

                while (enabled_.load()) {
                    SetNonce(blkStream, nonce);

                    if (nonce >= (i - nthreads) * task->step) {
                        timestamp = time(nullptr);
                        SetTimestamp(blkStream, timestamp);
                    }

                    ctx->SetHeader(blkStream.data(), blkStream.size());

                    if (aborted_ || task->abort_.load()) {
                        delete ctx;
                        return;
                    }

                    if (ctx->solve()) {
                        std::vector<uint32_t> sol(ctx->sols.end() - task->cycle_length,
                                                  ctx->sols.end()); // the last solution
                        uint256 cyclehash = HashBLAKE2<256>(sol.data(), (size_t) task->cycle_length * sizeof(word_t));
                        if (UintToArith256(cyclehash) <= task->target) {
                            solutions.Put({task->id, {timestamp, nonce, std::move(sol)}});
                            spdlog::trace("Found solution: thread {}, nonce {}, time {}, cycle hash {}", i, nonce,
                                          timestamp, cyclehash.to_substr());
                            delete ctx;
                            break;
                        }
                    }

                    nonce += nthreads * task->step;
                }
            });
        }

        // Block the main thread until a nonce is solved
        Solution last_result;
        while (solutions.Take(last_result)) {
            if (last_result.first == task->id) {
                // Abort unfinished tasks
                aborted_ = true;
                solutions.Quit();
                break;
            }
        }

        solverPool_.ClearAndDisableTasks();
        for (const auto& ctx : ctx_q) {
            if (ctx) {
                ctx->abort();
            }
        }
        solverPool_.Abort();

        if (!enabled_) {
            status.second = SolverResult::ErrorCode ::SERVER_ABORT;
        } else if (task->abort_) {
            status.second = SolverResult::ErrorCode ::TASK_CANCELED_BY_CLIENT;
        } else {
            status.first = std::make_unique<TaskResult>();

            status.first->final_time  = std::get<0>(last_result.second);
            status.first->final_nonce = std::get<1>(last_result.second);
            status.first->proof       = std::move(std::get<2>(last_result.second));
            status.second             = SolverResult::ErrorCode::SUCCESS;
        }

    } catch (std::exception& e) {
        status.second = SolverResult::ErrorCode ::UNKNOWN_ERROR;
    }
    return status;
}

void SolverManager::AbortTask(uint32_t task_id) {
    auto it = tasks.find(task_id);
    if (it != tasks.end()) {
        it->second->abort_ = true;
    }

    solutions.Quit();
}
