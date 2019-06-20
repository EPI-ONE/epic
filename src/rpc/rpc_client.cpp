#include <rpc_client.h>

RPCClient::RPCClient(std::shared_ptr<grpc::Channel> channel) : stub_(BasicBlockExplorerRPC::NewStub(channel)) {}

rpc::Block RPCClient::GetBlock(const uint256& block_hash) {
    GetBlockRequest request;
    rpc::Hash* h = HashToRPCHash(block_hash);
    request.set_allocated_hash(h);

    GetBlockResponse reply;
    grpc::ClientContext context;
    grpc::Status status = stub_->GetBlock(&context, request, &reply);

    if (!status.ok()) {
        spdlog::info("No response from RPC server");
    }
    return reply.block();
}

int RPCClient::GetLevelSet() {
    GetLevelSetRequest request;
    GetLevelSetResponse reply;
    grpc::ClientContext context;
    grpc::Status status = stub_->GetLevelSet(&context, request, &reply);
    return status.ok();
}

int RPCClient::GetLevelSetSize() {
    GetLevelSetSizeRequest request;
    GetLevelSetSizeResponse reply;
    grpc::ClientContext context;
    grpc::Status status = stub_->GetLevelSetSize(&context, request, &reply);
    return status.ok();
}

int RPCClient::GetNewMilestoneSince() {
    GetNewMilestoneSinceRequest request;
    GetNewMilestoneSinceResponse reply;
    grpc::ClientContext context;
    grpc::Status status = stub_->GetNewMilestoneSince(&context, request, &reply);
    return status.ok();
}

int RPCClient::GetLatestMilestone() {
    GetLatestMilestoneRequest request;
    GetLatestMilestoneResponse reply;
    grpc::ClientContext context;
    grpc::Status status = stub_->GetLatestMilestone(&context, request, &reply);
    return status.ok();
}
