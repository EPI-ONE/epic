#ifndef __SRC_RPC_SERVER_H__
#define __SRC_RPC_SERVER_H__

#include <grpc++/grpc++.h>
#include <rpc.grpc.pb.h>
#include <rpc.pb.h>
#include <rpc_tools.h>
#include <thread>

#include "block.h"
#include "init.h"
#include "miner.h"
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

using rpc::CommanderRPC;
using rpc::StartMinerRequest;
using rpc::StartMinerResponse;
using rpc::StatusRequest;
using rpc::StatusResponse;
using rpc::StopMinerRequest;
using rpc::StopMinerResponse;
using rpc::StopRequest;
using rpc::StopResponse;

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

class CommanderRPCServiceImpl final : public CommanderRPC::Service {
    grpc::Status Status(grpc::ServerContext* context, const StatusRequest* request, StatusResponse* reply) override;

    grpc::Status Stop(grpc::ServerContext* context, const StopRequest* request, StopResponse* reply) override;

    grpc::Status StartMiner(grpc::ServerContext* context,
                            const StartMinerRequest* request,
                            StartMinerResponse* reply) override;

    grpc::Status StopMiner(grpc::ServerContext* context,
                           const StopMinerRequest* request,
                           StopMinerResponse* reply) override;
};

class RPCServer {
public:
    RPCServer() = delete;

    RPCServer(NetAddress);

    void Start();

    void Shutdown();

    bool IsRunning();

private:
    std::string server_address_;
    std::atomic<bool> isRunning_;

    void LaunchServer();
};

extern std::unique_ptr<RPCServer> RPC;
#endif //__SRC_RPC_SERVER_H__
