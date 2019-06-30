#include <rpc_server.h>

grpc::Status BasicBlockExplorerRPCServiceImpl::GetBlock(grpc::ServerContext* context,
                                                        const GetBlockRequest* request,
                                                        GetBlockResponse* reply) {
    auto record   = DAG->GetState(RPCHashToHash(request->hash()));
    rpc::Block* b = BlockToRPCBlock(*(record->cblock));
    reply->set_allocated_block(b);
    return grpc::Status::OK;
}

grpc::Status BasicBlockExplorerRPCServiceImpl::GetLevelSet(grpc::ServerContext* context,
                                                           const GetLevelSetRequest* request,
                                                           GetLevelSetResponse* reply) {
    auto record = DAG->GetState(RPCHashToHash(request->hash()));
    auto ls     = DAG->GetMainChainLevelSet(record->height);
    for (auto localBlock : ls) {
        auto newBlock   = reply->add_blocks();
        auto blockValue = BlockToRPCBlock(*localBlock);
        *newBlock       = *blockValue;
        delete blockValue;
    }
    return grpc::Status::OK;
}

grpc::Status BasicBlockExplorerRPCServiceImpl::GetLevelSetSize(grpc::ServerContext* context,
                                                               const GetLevelSetSizeRequest* request,
                                                               GetLevelSetSizeResponse* reply) {
    auto record = DAG->GetState(RPCHashToHash(request->hash()));
    auto ls     = DAG->GetMainChainLevelSet(record->height);
    reply->set_size(ls.size());
    return grpc::Status::OK;
}

grpc::Status BasicBlockExplorerRPCServiceImpl::GetNewMilestoneSince(grpc::ServerContext* context,
                                                                    const GetNewMilestoneSinceRequest* request,
                                                                    GetNewMilestoneSinceResponse* reply) {
    auto record = DAG->GetState(RPCHashToHash(request->hash()));
    // Ignore the first milestone hash from traverse since it's the request's one
    auto milestone_hashes = DAG->TraverseMilestoneForward(record, request->number() + 1);
    for (size_t i = 1; i < milestone_hashes.size(); ++i) {
        auto rec        = DAG->GetState(milestone_hashes[i]);
        auto newBlock   = reply->add_blocks();
        auto blockValue = BlockToRPCBlock(*(rec->cblock));
        *newBlock       = *blockValue;
        delete blockValue;
    }
    return grpc::Status::OK;
}

grpc::Status BasicBlockExplorerRPCServiceImpl::GetLatestMilestone(grpc::ServerContext* context,
                                                                  const GetLatestMilestoneRequest* request,
                                                                  GetLatestMilestoneResponse* reply) {
    auto record   = DAG->GetMilestoneHead();
    rpc::Block* b = BlockToRPCBlock(*(record->cblock));
    reply->set_allocated_milestone(b);
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
