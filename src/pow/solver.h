// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef EPIC_SOLVER_H
#define EPIC_SOLVER_H

#include "block.h"
#include "concurrent_container.h"
#include "service/rpc_service.h"
#include "threadpool.h"

#include <grpcpp/support/status.h>

class Solver {
public:
    Solver()          = default;
    virtual ~Solver() = default;

    virtual bool Start()       = 0;
    virtual bool Stop()        = 0;
    virtual void Abort()       = 0;
    virtual bool Solve(Block&) = 0;

    uint32_t GetTaskID() {
        current_task_id.fetch_add(1);
        return current_task_id.load();
    }

protected:
    std::atomic_bool enabled = false;
    std::atomic_bool aborted = false;

    std::atomic_uint32_t current_task_id = 0;
    ConcurrentQueue<std::pair<uint32_t, std::tuple<uint32_t, uint32_t, std::vector<uint32_t>>>> task_results;
};

class CPUSolver : public Solver {
public:
    CPUSolver(size_t nThreads) : Solver() {
        solverPool_.SetThreadSize(nThreads);
    }

    ~CPUSolver() override {
        solverPool_.Stop();
    }

    bool Start() override;
    bool Stop() override;
    void Abort() override;
    bool Solve(Block&) override;

private:
    ThreadPool solverPool_;
};

class SolverRPCClient {
public:
    explicit SolverRPCClient(const std::string& address)
        : stub(grpc::CreateChannel(address, grpc::InsecureChannelCredentials())) {}

    grpc::Status SendTask(grpc::ClientContext* context, POWTask request, POWResult* reply) {
        return stub.SendPOWTask(context, request, reply);
    }

    grpc::Status AbortTask(grpc::ClientContext* context, StopTaskRequest request, StopTaskResponse* reply) {
        return stub.StopTask(context, request, reply);
    }

private:
    RemoteSolver::Stub stub;
};

class RemoteGPUSolver : public Solver {
public:
    using Solver::Solver;

    bool Start() override;
    bool Stop() override;
    void Abort() override;
    bool Solve(Block&) override;

private:
    std::unique_ptr<SolverRPCClient> client;
    std::atomic_bool sent_task_ = false;
};

#endif // EPIC_SOLVER_H
