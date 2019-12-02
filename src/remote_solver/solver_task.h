// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef EPIC_SOLVER_TASK_H
#define EPIC_SOLVER_TASK_H

#include "arith_uint256.h"
#include "cuckaroo.h"
#include "solver_protocol.h"
#include "stream.h"
#include "threadpool.h"
#include "trimmer.h"

struct TaskResult {
    uint32_t final_nonce;
    uint32_t final_time;
    std::vector<word_t> proof;
};

struct SolverTask {
    // task meta data
    uint32_t id;
    std::atomic_bool abort_ = false;

    // task parameters
    uint32_t init_nonce;
    uint32_t init_time;
    uint32_t step;
    uint32_t cycle_length;
    VStream blockHeader;
    arith_uint256 target;
};

using TaskStatus = std::pair<std::unique_ptr<TaskResult>, SolverResult::ErrorCode>;

#endif // EPIC_SOLVER_TASK_H
