// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "solver.h"
#include "config.h"
#include "cpptoml.h"
#include "net_address.h"
#include "solver_protocol.h"

inline void SetNonce(VStream& vs, uint32_t nonce) {
    memcpy(vs.data() + vs.size() - sizeof(uint32_t), &nonce, sizeof(uint32_t));
}

inline void SetTimestamp(VStream& vs, uint32_t t) {
    memcpy(vs.data() + vs.size() - 3 * sizeof(uint32_t), &t, sizeof(uint32_t));
}

bool CPUSolver::Start() {
    bool flag = false;
    if (enabled.compare_exchange_strong(flag, true)) {
        solverPool_.Start();
        return true;
    }

    return false;
}

bool CPUSolver::Stop() {
    bool flag = true;
    if (enabled.compare_exchange_strong(flag, false)) {
        solverPool_.Stop();
        return true;
    }

    return false;
}

void CPUSolver::Abort() {
    aborted = true;
}

bool CPUSolver::Solve(Block& b) {
    arith_uint256 target = b.GetTargetAsInteger();
    size_t nthreads      = solverPool_.GetThreadSize();

    final_nonce = 0;
    final_time  = b.GetTime();
    found_sols  = false;
    VStream vs(b.GetHeader());

    for (std::size_t i = 0; i < nthreads; ++i) {
        solverPool_.Execute([&, i]() {
            auto blkStream     = vs;
            uint32_t nonce     = b.GetNonce() + i - nthreads;
            uint32_t timestamp = final_time.load();

            while (enabled.load()) {
                nonce += nthreads;
                SetNonce(blkStream, nonce);

                if (nonce >= i - nthreads) {
                    timestamp = time(nullptr);
                    SetTimestamp(blkStream, timestamp);
                }

                if (found_sols.load() || aborted.load()) {
                    return;
                }

                uint256 blkHash = HashBLAKE2<256>(blkStream.data(), blkStream.size());
                if (UintToArith256(blkHash) <= target) {
                    bool f = false;
                    if (found_sols.compare_exchange_strong(f, true)) {
                        final_nonce = nonce;
                        final_time  = timestamp;
                    }
                    break;
                }
            }
        });
    }

    // Block the main thread until a nonce is solved
    while (!found_sols.load() && enabled.load() && !aborted.load()) {
        std::this_thread::yield();
    }

    solverPool_.Abort();

    b.SetNonce(final_nonce.load());
    b.SetTime(final_time.load());
    b.CalculateHash();
    b.CalculateOptimalEncodingSize();
    return true;
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
    StopTaskRequest request;
    StopTaskResponse response;
    request.set_task_id(current_task_id_);
    client->AbortTask(&context, request, &response);
}

bool RemoteGPUSolver::Solve(Block& b) {
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
    spdlog::debug("Sending solver task: id = {}", request.task_id());
    auto status = client->SendTask(&context, request, &reply);

    bool flag = false;
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
                flag = true;
                break;
            }
            case SolverResult::ErrorCode::SERVER_ABORT: {
                spdlog::warn("Remote solver aborted. Task failed: id = {}", request.task_id());
                break;
            }
            case SolverResult::ErrorCode::TASK_CANCELED_BY_CLIENT: {
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
