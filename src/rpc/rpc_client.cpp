#include <rpc_client.h>

RPCClient::RPCClient(std::shared_ptr<grpc::Channel> channel) : stub_(BasicBlockExplorerRPC::NewStub(channel)) {}

std::optional<rpc::Block> RPCClient::GetBlock(const uint256& block_hash) {
    GetBlockRequest request;
    rpc::Hash* h = HashToRPCHash(block_hash);
    request.set_allocated_hash(h);

    GetBlockResponse reply;
    grpc::ClientContext context;
    grpc::Status status = stub_->GetBlock(&context, request, &reply);

    if (!status.ok()) {
        spdlog::error("No response from RPC server: {}", status.error_message());
        return {};
    }
    return {reply.block()};
}

std::optional<std::vector<rpc::Block>> RPCClient::GetLevelSet(const uint256& block_hash) {
    GetLevelSetRequest request;
    rpc::Hash* h = HashToRPCHash(block_hash);
    request.set_allocated_hash(h);

    GetLevelSetResponse reply;
    grpc::ClientContext context;
    grpc::Status status = stub_->GetLevelSet(&context, request, &reply);
    if (!status.ok()) {
        spdlog::error("No response from RPC server: {}", status.error_message());
        return {};
    }
    auto n = reply.blocks_size();
    std::vector<rpc::Block> r;
    r.resize(n);
    for (size_t i = 0; i < n; ++i) {
        r[i] = reply.blocks(i);
    }
    return {r};
}

std::optional<size_t> RPCClient::GetLevelSetSize(const uint256& block_hash) {
    GetLevelSetSizeRequest request;
    rpc::Hash* h = HashToRPCHash(block_hash);
    request.set_allocated_hash(h);

    GetLevelSetSizeResponse reply;
    grpc::ClientContext context;
    grpc::Status status = stub_->GetLevelSetSize(&context, request, &reply);
    if (!status.ok()) {
        spdlog::error("No response from RPC server: {}", status.error_message());
        return {};
    }
    return {reply.size()};
}

std::optional<rpc::Block> RPCClient::GetLatestMilestone() {
    GetLatestMilestoneRequest request;
    GetLatestMilestoneResponse reply;
    grpc::ClientContext context;
    grpc::Status status = stub_->GetLatestMilestone(&context, request, &reply);
    if (!status.ok()) {
        spdlog::error("No response from RPC server: {}", status.error_message());
        return {};
    }
    return {reply.milestone()};
}

std::optional<std::vector<rpc::Block>> RPCClient::GetNewMilestoneSince(const uint256& block_hash,
                                                                       size_t numberOfMilestone) {
    GetNewMilestoneSinceRequest request;
    rpc::Hash* h = HashToRPCHash(block_hash);
    request.set_allocated_hash(h);
    request.set_number(numberOfMilestone);

    GetNewMilestoneSinceResponse reply;
    grpc::ClientContext context;
    grpc::Status status = stub_->GetNewMilestoneSince(&context, request, &reply);
    if (!status.ok()) {
        spdlog::error("No response from RPC server: {}", status.error_message());
        return {};
    }

    auto n = reply.blocks_size();
    std::vector<rpc::Block> r;
    r.resize(n);
    for (size_t i = 0; i < n; ++i) {
        r[i] = reply.blocks(i);
    }
    return {r};
}
