#ifndef __SRC_RPC_SERVER_H__
#define __SRC_RPC_SERVER_H__

#include <grpc++/grpc++.h>
#include <rpc.grpc.pb.h>
#include <rpc.pb.h>
#include <rpc_tools.h>
#include <thread>

#include "block.h"
#include "net_address.h"
#include "spdlog.h"

using rpc::BasicBlockExplorerRPC;
using rpc::GetBlockRequest;
using rpc::GetBlockResponse;
using rpc::GetLatestMilestoneRequest;
using rpc::GetLatestMilestoneResponse;
using rpc::GetLevelSetRequest;
using rpc::GetLevelSetResponse;
using rpc::GetLevelSetSizeRequest;
using rpc::GetLevelSetSizeResponse;
using rpc::GetNewMilestoneSinceRequest;
using rpc::GetNewMilestoneSinceResponse;

class BasicBlockExplorerRPCServiceImpl final : public BasicBlockExplorerRPC::Service {
    grpc::Status GetBlock(grpc::ServerContext* context,
        const GetBlockRequest* request,
        GetBlockResponse* reply) override;

    grpc::Status GetLevelSet(grpc::ServerContext* context,
        const GetLevelSetRequest* request,
        GetLevelSetResponse* reply) override;

    grpc::Status GetLevelSetSize(grpc::ServerContext* context,
        const GetLevelSetSizeRequest* request,
        GetLevelSetSizeResponse* reply) override;

    grpc::Status GetNewMilestoneSince(grpc::ServerContext* context,
        const GetNewMilestoneSinceRequest* request,
        GetNewMilestoneSinceResponse* reply) override;

    grpc::Status GetLatestMilestone(grpc::ServerContext* context,
        const GetLatestMilestoneRequest* request,
        GetLatestMilestoneResponse* reply) override;
};

class RPCServer {
public:
    RPCServer() = delete;

    RPCServer(NetAddress);

    void Start();

    void Shutdown();

private:
    std::string server_address_;
    std::atomic<bool> keepRunning_;

    void LaunchServer();
};

#endif //__SRC_RPC_SERVER_H__
