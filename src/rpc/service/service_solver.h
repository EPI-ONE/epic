// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef EPIC_SOLVER_H
#define EPIC_SOLVER_H

#include "rpc.grpc.pb.h"
#include "rpc.pb.h"
#include "rpc_service.h"
#include "solver_manager.h"

#include <utility>
#include <grpc++/grpc++.h>

class SolverRPCServiceImpl final : public RemoteSolver::Service {
public:
    grpc::Status SendPOWTask(grpc::ServerContext* context, const POWTask* task, POWResult* result) override {
        auto solverTask = CreateTask(task);
        if (!solverTask) {
            result->set_error_code(SolverResult::ErrorCode::INVALID_PARAM);
        } else {
            auto calResult = blockSolver_->ProcessTask(solverTask);
            result->set_error_code(calResult.second);
            if (calResult.second == SolverResult::ErrorCode::SUCCESS) {
                for (auto& proof_element : calResult.first->proof) {
                    result->add_proof(proof_element);
                    result->set_nonce(calResult.first->final_nonce);
                    result->set_time(calResult.first->final_time);
                }
            }
        }
        return grpc::Status::OK;
    }

    grpc::Status StopTask(grpc::ServerContext* context,
                          const StopTaskRequest* request,
                          StopTaskResponse* reply) override {
        blockSolver_->AbortTask(request->task_id());
        return grpc::Status::OK;
    }

    explicit SolverRPCServiceImpl(std::shared_ptr<SolverManager> solver) : blockSolver_(std::move(solver)) {}

private:
    std::shared_ptr<SolverManager> blockSolver_;
    bool checkParams(const POWTask* task) {
        if (!task) {
            return false;
        }
        if (task->cycle_length() == 0) {
            return false;
        }
        if (task->header().empty()) {
            return false;
        }
        if (task->target().empty()) {
            return false;
        }
        if (task->step() == 0) {
            return false;
        }
        return true;
    }

    std::shared_ptr<SolverTask> CreateTask(const POWTask* task) {
        if (!checkParams(task)) {
            return nullptr;
        }
        auto solverTask          = std::make_shared<SolverTask>();
        solverTask->id           = task->task_id();
        solverTask->init_nonce   = task->init_nonce();
        solverTask->init_time    = task->init_time();
        solverTask->step         = task->step() == 0 ? 1 : task->step();
        solverTask->cycle_length = task->cycle_length();
        solverTask->blockHeader  = VStream(task->header().data(), task->header().data() + task->header().length());
        solverTask->target.SetHex(task->target());

        return solverTask;
    }
};


#endif // EPIC_SOLVER_H
