// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "solver.h"
#include "config.h"
#include "cpptoml.h"
#include "net_address.h"
#include "remote_solver/solver_protocol.h"

inline void SetNonce(VStream& vs, uint32_t nonce) {
    memcpy(vs.data() + vs.size() - sizeof(uint32_t), &nonce, sizeof(uint32_t));
}

inline void SetTimestamp(VStream& vs, uint32_t t) {
    memcpy(vs.data() + vs.size() - 3 * sizeof(uint32_t), &t, sizeof(uint32_t));
}

bool CPUSolver::Start() {
    solverPool_.Start();
    return true;
}

bool CPUSolver::Stop() {
    Abort();
    solverPool_.Stop();
    return true;
}

void CPUSolver::Abort() {
    enabled = false;
    solutions.Quit();
}

void CPUSolver ::Enable() {
    enabled = true;
    solutions.Enable();
}

int CPUSolver::Solve(Block& b) {
    Enable();

    size_t nthreads = solverPool_.GetThreadSize();
    auto task_id    = GetTaskID();
    VStream vs(b.GetHeader());

    for (std::size_t i = 0; i < nthreads; ++i) {
        solverPool_.Execute([i, nthreads, target = b.GetTargetAsInteger(), task_id, nonce = b.GetNonce() + i,
                             timestamp = b.GetTime(), vs, this]() mutable {
            while (enabled.load()) {
                SetNonce(vs, nonce);

                if (nonce >= i - nthreads) {
                    timestamp = time(nullptr);
                    SetTimestamp(vs, timestamp);
                }

                uint256 blkHash = HashBLAKE2<256>(vs.data(), vs.size());
                if (UintToArith256(blkHash) <= target) {
                    solutions.Put({task_id, {timestamp, nonce, std::vector<uint32_t>{}}});
                    break;
                }

                nonce += nthreads;
            }
        });
    }

    // Block the main thread until a nonce is solved
    Solution last_result;
    while (solutions.Take(last_result)) {
        if (last_result.first == task_id) {
            Abort();
            b.SetTime(std::get<0>(last_result.second));
            b.SetNonce(std::get<1>(last_result.second));
            b.CalculateHash();
            b.CalculateOptimalEncodingSize();

            break;
        }
    }

    solverPool_.Abort();

    if (enabled) {
        return SolverResult::ErrorCode::SUCCESS;
    } else {
        return SolverResult::ErrorCode::SERVER_ABORT;
    }
}

bool RemoteGPUSolver::Start() {
    // Read config for remote solver socket
    auto miner_config = cpptoml::parse_file(CONFIG->GetConfigFilePath())->get_table("miner");
    if (miner_config) {
        auto solver_path = miner_config->get_as<std::string>("solver_addr");
        if (solver_path) {
            CONFIG->SetSolverAddr(*solver_path);
        } else {
            spdlog::error("Error parsing remote solver socket.");
            return false;
        }
    } else {
        spdlog::error("No config found for solver.");
        return false;
    }

    if (!enabled.load()) {
        auto client_ip = NetAddress::GetByIP(CONFIG->GetSolverAddr());
        if (client_ip) {
            client  = std::make_unique<SolverRPCClient>(client_ip->ToString());
            enabled = true;
            return true;
        }
        spdlog::error("Invalid solver RPC client ip. Failed to start the miner.");
    }

    return false;
}

bool RemoteGPUSolver::Stop() {
    enabled = false;
    client.reset();
    sent_task_ = false;
    return true;
}

void RemoteGPUSolver::Abort() {
    if (!sent_task_) {
        return;
    }

    grpc::ClientContext context;
    rpc::StopTaskRequest request;
    rpc::StopTaskResponse response;
    request.set_task_id(current_task_id);
    client->AbortTask(&context, request, &response);
}

int RemoteGPUSolver::Solve(Block& b) {
    arith_uint256 target = b.GetTargetAsInteger();
    VStream vs(b.GetHeader());
    grpc::ClientContext context;
    rpc::POWTask request;
    rpc::POWResult reply;
    request.set_task_id(GetTaskID());
    request.set_init_nonce(0);
    request.set_init_time(b.GetTime());
    request.set_step(1);
    request.set_cycle_length(GetParams().cycleLen);
    request.set_target(target.GetHex());
    request.set_header(vs.data(), vs.size());

    if (!sent_task_) {
        sent_task_ = true;
    }
    spdlog::debug("Sending solver task: id = {}", request.task_id());
    auto status = client->SendTask(&context, request, &reply);

    if (status.ok()) {
        switch (reply.error_code()) {
            case SolverResult::ErrorCode::SUCCESS: {
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
                break;
            }
            case SolverResult::ErrorCode::SERVER_ABORT: {
                spdlog::warn("Remote solver aborted. Task failed: id = {}", request.task_id());
                break;
            }
            case SolverResult::ErrorCode::TASK_CANCELED_BY_CLIENT: {
                spdlog::warn("We canceled this task: id = {}", request.task_id());
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
        spdlog::warn("RPC error. Task failed: id = {}, error message = {}", request.task_id(), status.error_message());
        return SolverResult::ErrorCode::REMOTE_DISCONNECT;
    }
    return reply.error_code();
}
