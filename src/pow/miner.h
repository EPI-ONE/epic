// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef EPIC_MINER_H
#define EPIC_MINER_H

#include "block_store.h"
#include "circular_queue.h"
#include "key.h"
#include "mempool.h"
#include "peer_manager.h"
#include "service/rpc_service.h"
#include "trimmer.h"
#include <atomic>
#include <grpcpp/support/status.h>

template <typename>
class CircularQueue;

class MinerRPCClient {
public:
    explicit MinerRPCClient(const std::string& address)
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

class Miner {
public:
    Miner();
    virtual ~Miner() {}

    virtual bool Start();
    virtual bool Stop();
    virtual bool Solve(Block&);
    void Run();

    void AbortTask(uint32_t task_id);

    bool IsRunning() {
        return enabled_.load();
    }

    ConstBlockPtr GetSelfChainHead() const {
        return selfChainHead_;
    }

protected:
    std::atomic_bool enabled_ = false;
    std::atomic_bool abort_   = false;
    std::thread runner_;
    std::thread inspector_;

private:
    ConstBlockPtr selfChainHead_ = nullptr;
    VertexPtr chainHead_         = nullptr;
    Cumulator distanceCal_;
    CircularQueue<uint256> selfChainHeads_;
    std::unique_ptr<MinerRPCClient> client;

    std::atomic<uint32_t> current_task_id_;
    std::atomic_bool sent_task_{false};
    uint256 SelectTip();
};

extern std::unique_ptr<Miner> MINER;

inline void SetNonce(VStream& vs, uint32_t nonce) {
    memcpy(vs.data() + vs.size() - sizeof(uint32_t), &nonce, sizeof(uint32_t));
}

inline void SetTimestamp(VStream& vs, uint32_t t) {
    memcpy(vs.data() + vs.size() - 3 * sizeof(uint32_t), &t, sizeof(uint32_t));
}

#endif // EPIC_MINER_H
