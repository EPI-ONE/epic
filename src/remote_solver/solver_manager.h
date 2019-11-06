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
    std::atomic_bool aborted_ = false;
    BlockingQueue<uint32_t> task_queue_;
    ConcurrentHashMap<uint32_t, std::shared_ptr<SolverTask>> tasks;
    std::atomic_bool isIdle = true;

    ThreadPool solverPool_;
    SolverParams solverParams_;

    using Solution = std::pair<uint32_t, std::tuple<uint32_t, uint32_t, std::vector<uint32_t>>>;
    BlockingQueue<Solution> solutions;
};


#endif // EPIC_SOLVER_MANAGER_H
