// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef EPIC_SOLVER_MANAGER_H
#define EPIC_SOLVER_MANAGER_H

#include "concurrent_container.h"
#include "solver_protocol.h"
#include "solver_task.h"
#include "threadpool.h"


class SolverManager {
public:
    explicit SolverManager(size_t nThreads);
    TaskStatus ProcessTask(const std::shared_ptr<SolverTask>& task);
    TaskStatus Solve(std::shared_ptr<SolverTask> task);
    void AbortTask(uint32_t task_id);

    bool Start();
    bool Stop();

private:
    std::atomic_bool enabled_ = false;
    BlockingQueue<uint32_t> task_queue_;
    ConcurrentHashMap<uint32_t, std::shared_ptr<SolverTask>> tasks;
    std::atomic_bool isIdle = true;

    ThreadPool solverPool_;
    SolverParams solverParams_;
    std::atomic<uint32_t> final_nonce;
    std::atomic<uint64_t> final_time;
    std::atomic<bool> found_sols;
};


#endif // EPIC_SOLVER_MANAGER_H
