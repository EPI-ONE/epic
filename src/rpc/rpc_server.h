#include <rpc.grpc.pb.h>
#include <rpc.pb.h>

#include <grpc++/grpc++.h>

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
        GetBlockResponse* reply) override {
        return grpc::Status::OK;
    }

    grpc::Status GetLevelSet(grpc::ServerContext* context,
        const GetLevelSetRequest* request,
        GetLevelSetResponse* reply) override {
        return grpc::Status::OK;
    }

    grpc::Status GetLevelSetSize(grpc::ServerContext* context,
        const GetLevelSetSizeRequest* request,
        GetLevelSetSizeResponse* reply) override {
        return grpc::Status::OK;
    }

    grpc::Status GetNewMilestoneSince(grpc::ServerContext* context,
        const GetNewMilestoneSinceRequest* request,
        GetNewMilestoneSinceResponse* reply) override {
        return grpc::Status::OK;
    }

    grpc::Status GetLatestMilestone(grpc::ServerContext* context,
        const GetLatestMilestoneRequest* request,
        GetLatestMilestoneResponse* reply) override {
        return grpc::Status::OK;
    }
};

void RunRPCServer() {
    BasicBlockExplorerRPCServiceImpl service;
    grpc::ServerBuilder builder;
    std::string server_address("0.0.0.0:777");
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
    std::cout << "Server is running on " << server_address << std::endl;
    server->Wait();
}
