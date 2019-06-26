#include <rpc_server.h>

grpc::Status BasicBlockExplorerRPCServiceImpl::GetBlock(grpc::ServerContext* context,
    const GetBlockRequest* request,
    GetBlockResponse* reply) {
    if (request->hash().hash() == std::to_string(GENESIS.GetHash())) {
        rpc::Block* b = BlockToRPCBlock(GENESIS);
        reply->set_allocated_block(b);
    }
    return grpc::Status::OK;
}

grpc::Status BasicBlockExplorerRPCServiceImpl::GetLevelSet(grpc::ServerContext* context,
    const GetLevelSetRequest* request,
    GetLevelSetResponse* reply) {
    return grpc::Status::OK;
}

grpc::Status BasicBlockExplorerRPCServiceImpl::GetLevelSetSize(grpc::ServerContext* context,
    const GetLevelSetSizeRequest* request,
    GetLevelSetSizeResponse* reply) {
    return grpc::Status::OK;
}

grpc::Status BasicBlockExplorerRPCServiceImpl::GetNewMilestoneSince(grpc::ServerContext* context,
    const GetNewMilestoneSinceRequest* request,
    GetNewMilestoneSinceResponse* reply) {
    return grpc::Status::OK;
}

grpc::Status BasicBlockExplorerRPCServiceImpl::GetLatestMilestone(grpc::ServerContext* context,
    const GetLatestMilestoneRequest* request,
    GetLatestMilestoneResponse* reply) {
    return grpc::Status::OK;
}

RPCServer::RPCServer(NetAddress adr) {
    this->server_address_ = adr.ToString();
}

void RPCServer::Start() {
    keepRunning_ = true;
    std::thread t(&RPCServer::LaunchServer, this);
    t.detach();
}

void RPCServer::Shutdown() {
    keepRunning_ = false;
    spdlog::info("RPC Server is shutting down");
}

void RPCServer::LaunchServer() {
    BasicBlockExplorerRPCServiceImpl service;
    grpc::ServerBuilder builder;
    builder.AddListeningPort(this->server_address_, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
    spdlog::info("RPC Server is running on {}", this->server_address_);
    while (keepRunning_) {
        std::this_thread::yield();
    }
    server->Shutdown();
}
