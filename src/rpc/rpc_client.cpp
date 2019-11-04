// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "rpc_client.h"
#include "spdlog/spdlog.h"

rpc::Hash* HashToRPCHash(std::string h) {
    rpc::Hash* rpch = new rpc::Hash();
    rpch->set_hash(h);
    return rpch;
}

RPCClient::RPCClient(std::shared_ptr<grpc::Channel> channel)
    : be_stub_(BasicBlockExplorerRPC::NewStub(channel)), commander_stub_(CommanderRPC::NewStub(channel)) {}

std::optional<rpc::Block> RPCClient::GetBlock(std::string block_hash) {
    GetBlockRequest request;
    rpc::Hash* h = HashToRPCHash(block_hash);
    request.set_allocated_hash(h);

    GetBlockResponse response;
    grpc::ClientContext context;
    grpc::Status status = be_stub_->GetBlock(&context, request, &response);

    if (!status.ok()) {
        spdlog::error("No response from RPC server: {}", status.error_message());
        return {};
    }
    return {response.block()};
}

std::optional<std::vector<rpc::Block>> RPCClient::GetLevelSet(std::string block_hash) {
    GetLevelSetRequest request;
    rpc::Hash* h = HashToRPCHash(block_hash);
    request.set_allocated_hash(h);

    GetLevelSetResponse response;
    grpc::ClientContext context;
    grpc::Status status = be_stub_->GetLevelSet(&context, request, &response);
    if (!status.ok()) {
        spdlog::error("No response from RPC server: {}", status.error_message());
        return {};
    }
    if (auto n = response.blocks_size()) {
        std::vector<rpc::Block> r;
        r.resize(n);
        for (size_t i = 0; i < n; ++i) {
            r[i] = response.blocks(i);
        }
        return {r};
    }
    return {};
}

std::optional<size_t> RPCClient::GetLevelSetSize(std::string block_hash) {
    GetLevelSetSizeRequest request;
    rpc::Hash* h = HashToRPCHash(block_hash);
    request.set_allocated_hash(h);

    GetLevelSetSizeResponse response;
    grpc::ClientContext context;
    grpc::Status status = be_stub_->GetLevelSetSize(&context, request, &response);
    if (!status.ok()) {
        spdlog::error("No response from RPC server: {}", status.error_message());
        return {};
    }
    return {response.size()};
}

std::optional<rpc::Block> RPCClient::GetLatestMilestone() {
    GetLatestMilestoneRequest request;
    GetLatestMilestoneResponse response;
    grpc::ClientContext context;
    grpc::Status status = be_stub_->GetLatestMilestone(&context, request, &response);
    if (!status.ok()) {
        spdlog::error("No response from RPC server: {}", status.error_message());
        return {};
    }
    return {response.milestone()};
}

std::optional<std::vector<rpc::Block>> RPCClient::GetNewMilestoneSince(std::string block_hash,
                                                                       size_t numberOfMilestone) {
    GetNewMilestoneSinceRequest request;
    rpc::Hash* h = HashToRPCHash(block_hash);
    request.set_allocated_hash(h);
    request.set_number(numberOfMilestone);

    GetNewMilestoneSinceResponse response;
    grpc::ClientContext context;
    grpc::Status status = be_stub_->GetNewMilestoneSince(&context, request, &response);
    if (!status.ok()) {
        spdlog::error("No response from RPC server: {}", status.error_message());
        return {};
    }

    if (auto n = response.blocks_size()) {
        std::vector<rpc::Block> blocks;
        blocks.resize(n);
        for (size_t i = 0; i < n; ++i) {
            blocks[i] = response.blocks(i);
        }
        return {blocks};
    }
    return {};
}

std::optional<StatusResponse> RPCClient::Status() {
    StatusRequest request;
    StatusResponse response;
    grpc::ClientContext context;
    grpc::Status status = commander_stub_->Status(&context, request, &response);
    if (!status.ok()) {
        spdlog::error("No response from RPC server: {}", status.error_message());
        return {};
    }
    return {response};
}

bool RPCClient::Stop() {
    StopRequest request;
    StopResponse response;
    grpc::ClientContext context;
    grpc::Status status = commander_stub_->Stop(&context, request, &response);
    if (!status.ok()) {
        spdlog::error("No response from RPC server: {}", status.error_message());
        return false;
    }
    return true;
}

std::optional<bool> RPCClient::StartMiner() {
    StartMinerRequest request;
    StartMinerResponse response;
    grpc::ClientContext context;
    grpc::Status status = commander_stub_->StartMiner(&context, request, &response);
    if (!status.ok()) {
        spdlog::error("No response from RPC server: {}", status.error_message());
        return {};
    }
    return response.success();
}

RPCClient::option_string RPCClient::StopMiner() {
    StopMinerRequest request;
    StopMinerResponse response;
    grpc::ClientContext context;
    grpc::Status status = commander_stub_->StopMiner(&context, request, &response);
    if (!status.ok()) {
        spdlog::error("No response from RPC server: {}", status.error_message());
        return {};
    }
    return response.result();
}

RPCClient::option_string RPCClient::CreateFirstReg(std::string addr, bool force) {
    CreateFirstRegRequest request;
    CreateFirstRegResponse response;
    grpc::ClientContext context;
    request.set_address(addr);
    request.set_force(force);
    auto status = commander_stub_->CreateFirstReg(&context, request, &response);
    if (!status.ok()) {
        spdlog::error("No response from RPC server: {}", status.error_message());
        return {};
    }
    return response.result();
}

std::optional<std::string> RPCClient::CreateRandomTx(size_t size) {
    CreateRandomTxRequest request;
    CreateRandomTxResponse response;
    grpc::ClientContext context;
    request.set_size(size);
    auto status = commander_stub_->CreateRandomTx(&context, request, &response);
    if (!status.ok()) {
        spdlog::error("No response from RPC server: {}", status.error_message());
        return {};
    }
    return response.result();
}

RPCClient::option_string RPCClient::CreateTx(const std::vector<std::pair<uint64_t, std::string>>& outputs,
                                             uint64_t fee) {
    CreateTxRequest request;
    CreateTxResponse response;
    grpc::ClientContext context;

    request.set_fee(fee);
    for (auto& output : outputs) {
        auto rpc_output = request.add_outputs();
        rpc_output->set_address(output.second);
        rpc_output->set_money(output.first);
    }
    auto status = commander_stub_->CreateTx(&context, request, &response);
    if (!status.ok()) {
        spdlog::error("No response from RPC server: {}", status.error_message());
        return {};
    }
    return response.txinfo();
}

RPCClient::option_string RPCClient::GetBalance() {
    GetBalanceRequest request;
    GetBalanceResponse response;
    grpc::ClientContext context;

    auto status = commander_stub_->GetBalance(&context, request, &response);
    if (!status.ok()) {
        spdlog::error("No response from RPC server: {}", status.error_message());
        return {};
    }
    return response.coin();
}

RPCClient::option_string RPCClient::GenerateNewKey() {
    GenerateNewKeyRequest request;
    GenerateNewKeyResponse response;
    grpc::ClientContext context;

    auto status = commander_stub_->GenerateNewKey(&context, request, &response);
    if (!status.ok()) {
        spdlog::error("No response from RPC server: {}", status.error_message());
        return {};
    }

    return response.address();
}

RPCClient::option_string RPCClient::SetPassphrase(const std::string& passphrase) {
    SetPassphraseRequest request;
    SetPassphraseResponse response;
    grpc::ClientContext context;

    request.set_passphrase(passphrase);
    auto status = commander_stub_->SetPassphrase(&context, request, &response);
    if (!status.ok()) {
        spdlog::error("No response from RPC server: {}", status.error_message());
        return {};
    }

    return response.responseinfo();
}

RPCClient::option_string RPCClient::ChangePassphrase(const std::string& oldPassphrase,
                                                     const std::string& newPassphrase) {
    ChangePassphraseRequest request;
    ChangePassphraseResponse response;
    grpc::ClientContext context;

    request.set_oldpassphrase(oldPassphrase);
    request.set_newpassphrase(newPassphrase);
    auto status = commander_stub_->ChangePassphrase(&context, request, &response);
    if (!status.ok()) {
        spdlog::error("No response from RPC server: {}", status.error_message());
        return {};
    }

    return response.responseinfo();
}

RPCClient::option_string RPCClient::Login(const std::string& passphrase) {
    LoginRequest request;
    LoginResponse response;
    grpc::ClientContext context;

    request.set_passphrase(passphrase);
    auto status = commander_stub_->Login(&context, request, &response);
    if (!status.ok()) {
        spdlog::error("No response from RPC server: {}", status.error_message());
        return {};
    }
    return response.responseinfo();
}

RPCClient::option_string RPCClient::ConnectPeers(const std::vector<std::string>& addresses) {
    ConnectRequest request;
    ConnectResponse response;
    grpc::ClientContext context;

    if (addresses.empty()) {
        spdlog::warn("Please specify at least one address to connect");
        return {};
    }
    for (const auto& address : addresses) {
        request.add_address(address);
    }
    auto status = commander_stub_->ConnectPeer(&context, request, &response);
    if (!status.ok()) {
        spdlog::error("No response from RPC server: {}", status.error_message());
        return {};
    }
    return response.result();
}

RPCClient::option_string RPCClient::DisconnectPeer(std::string& address) {
    DisConnectPeerRequest request;
    DisConnectPeerResponse response;
    grpc::ClientContext context;

    request.set_address(address);
    auto status = commander_stub_->DisConnectPeer(&context, request, &response);
    if (!status.ok()) {
        spdlog::error("No response from RPC server: {}", status.error_message());
        return {};
    }
    return response.result();
}

RPCClient::option_string RPCClient::DisconnectAllPeers() {
    DisConnectAllRequest request;
    DisConnectAllResponse response;
    grpc::ClientContext context;

    auto status = commander_stub_->DisConnectAllPeers(&context, request, &response);
    if (!status.ok()) {
        spdlog::error("No response from RPC server: {}", status.error_message());
        return {};
    }
    return response.result();
}
