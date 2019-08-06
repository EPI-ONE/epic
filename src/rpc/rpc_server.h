#ifndef __SRC_RPC_SERVER_H__
#define __SRC_RPC_SERVER_H__


#include <rpc_tools.h>
#include <thread>

#include "block.h"
#include "init.h"
#include "miner.h"
#include "net_address.h"
#include "rpc_header.h"
#include "spdlog.h"
#include "wallet.h"


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

    grpc::Status CreateRandomTx(grpc::ServerContext* context,
                                const CreateRandomTxRequest* request,
                                CreateRandomTxResponse* reply) override;

    grpc::Status CreateTx(grpc::ServerContext* context,
                          const CreateTxRequest* request,
                          CreateTxResponse* reply) override;

    grpc::Status GenerateNewKey(grpc::ServerContext* context,
                                const GenerateNewKeyRequest* request,
                                GenerateNewKeyResponse* reply) override;

    grpc::Status GetBalance(grpc::ServerContext* context,
                            const GetBalanceRequest* request,
                            GetBalanceResponse* reply) override;
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
