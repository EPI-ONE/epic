// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "rpc_client.h"

#include <google/protobuf/util/json_util.h>

using namespace rpc;
using google::protobuf::util::MessageToJsonString;
using op_string = std::optional<std::string>;

google::protobuf::util::JsonPrintOptions GetOption() {
    static google::protobuf::util::JsonPrintOptions option;
    option.add_whitespace                = true;
    option.always_print_primitive_fields = true;
    option.preserve_proto_field_names    = true;
    return option;
}

template <typename OP, typename Request, typename Response>
bool ClientCallback(const OP& op, const Request& request, Response* response) {
    grpc::ClientContext context;
    grpc::Status status = op(&context, request, response);

    if (!status.ok()) {
        std::cout << "No response from RPC server: " << status.error_message() << std::endl;
        return false;
    }

    return true;
}

template <typename OP, typename Request, typename Response>
op_string ProcessResponse(const OP& op, const Request& request, Response* response) {
    if (!ClientCallback(op, request, response)) {
        return {};
    }

    std::string result;
    if (MessageToJsonString(*response, &result, GetOption()).ok()) {
        return result;
    } else {
        return {};
    }
}

RPCClient::RPCClient(std::shared_ptr<grpc::Channel> channel)
    : be_stub_(BasicBlockExplorerRPC::NewStub(channel)), commander_stub_(CommanderRPC::NewStub(channel)) {}

op_string RPCClient::GetBlock(std::string block_hash) {
    GetBlockRequest request;
    request.set_hash(block_hash);

    GetBlockResponse response;
    return ProcessResponse(
        [&](auto* context, const auto& request, auto* response) -> grpc::Status {
            return be_stub_->GetBlock(context, request, response);
        },
        request, &response);
}

op_string RPCClient::GetLevelSet(std::string block_hash) {
    GetLevelSetRequest request;
    request.set_hash(block_hash);

    GetLevelSetResponse response;
    return ProcessResponse(
        [&](auto* context, const auto& request, auto* response) -> grpc::Status {
            return be_stub_->GetLevelSet(context, request, response);
        },
        request, &response);
}

op_string RPCClient::GetLevelSetSize(std::string block_hash) {
    GetLevelSetSizeRequest request;
    request.set_hash(block_hash);

    GetLevelSetSizeResponse response;
    return ProcessResponse(
        [&](auto* context, const auto& request, auto* response) -> grpc::Status {
            return be_stub_->GetLevelSetSize(context, request, response);
        },
        request, &response);
}

op_string RPCClient::GetLatestMilestone() {
    EmptyRequest request;
    GetLatestMilestoneResponse response;
    return ProcessResponse(
        [&](auto* context, const auto& request, auto* response) -> grpc::Status {
            return be_stub_->GetLatestMilestone(context, request, response);
        },
        request, &response);
}

op_string RPCClient::GetNewMilestoneSince(std::string block_hash, size_t numberOfMilestone) {
    GetNewMilestoneSinceRequest request;
    request.set_hash(block_hash);
    request.set_number(numberOfMilestone);

    GetNewMilestoneSinceResponse response;
    return ProcessResponse(
        [&](auto* context, const auto& request, auto* response) -> grpc::Status {
            return be_stub_->GetNewMilestoneSince(context, request, response);
        },
        request, &response);
}

op_string RPCClient::GetVertex(std::string block_hash) {
    GetVertexRequest request;
    request.set_hash(block_hash);

    GetVertexResponse response;
    return ProcessResponse(
        [&](auto* context, const auto& request, auto* response) -> grpc::Status {
            return be_stub_->GetVertex(context, request, response);
        },
        request, &response);
}

op_string RPCClient::GetMilestone(std::string block_hash) {
    GetBlockRequest request;
    request.set_hash(block_hash);

    GetMilestoneResponse response;
    return ProcessResponse(
        [&](auto* context, const auto& request, auto* response) -> grpc::Status {
            return be_stub_->GetMilestone(context, request, response);
        },
        request, &response);
}

op_string RPCClient::GetForks() {
    EmptyRequest request;
    GetForksResponse response;
    return ProcessResponse(
        [&](auto* context, const auto& request, auto* response) -> grpc::Status {
            return be_stub_->GetForks(context, request, response);
        },
        request, &response);
}

op_string RPCClient::GetPeerChains() {
    EmptyRequest request;
    GetPeerChainsResponse response;
    return ProcessResponse(
        [&](auto* context, const auto& request, auto* response) -> grpc::Status {
            return be_stub_->GetPeerChains(context, request, response);
        },
        request, &response);
}

op_string RPCClient::GetRecentStat() {
    EmptyRequest request;
    GetRecentStatResponse response;
    return ProcessResponse(
        [&](auto* context, const auto& request, auto* response) -> grpc::Status {
            return be_stub_->GetRecentStat(context, request, response);
        },
        request, &response);
}

op_string RPCClient::Statistic() {
    EmptyRequest request;
    StatisticResponse response;
    return ProcessResponse(
        [&](auto* context, const auto& request, auto* response) -> grpc::Status {
            return be_stub_->Statistic(context, request, response);
        },
        request, &response);
}

op_string RPCClient::Status() {
    EmptyRequest request;
    StatusResponse response;
    return ProcessResponse(
        [&](auto* context, const auto& request, auto* response) -> grpc::Status {
            return commander_stub_->Status(context, request, response);
        },
        request, &response);
}

bool RPCClient::Stop() {
    EmptyRequest request;
    StopResponse response;
    grpc::ClientContext context;
    grpc::Status status = commander_stub_->Stop(&context, request, &response);
    if (!status.ok()) {
        std::cout << "No response from RPC server: " << status.error_message() << std::endl;
        return false;
    }
    return true;
}

std::optional<bool> RPCClient::StartMiner() {
    EmptyRequest request;
    StartMinerResponse response;

    if (!ClientCallback([&](auto* context, const auto& request, auto* response)
                            -> grpc::Status { return commander_stub_->StartMiner(context, request, response); },
                        request, &response)) {
        return {};
    }

    return response.success();
}

op_string RPCClient::StopMiner() {
    EmptyRequest request;
    StopMinerResponse response;

    if (!ClientCallback([&](auto* context, const auto& request, auto* response)
                            -> grpc::Status { return commander_stub_->StopMiner(context, request, response); },
                        request, &response)) {
        return {};
    }

    return response.result();
}

op_string RPCClient::CreateFirstReg(std::string addr, bool force) {
    CreateFirstRegRequest request;
    CreateFirstRegResponse response;

    request.set_address(addr);
    request.set_force(force);
    if (!ClientCallback([&](auto* context, const auto& request, auto* response)
                            -> grpc::Status { return commander_stub_->CreateFirstReg(context, request, response); },
                        request, &response)) {
        return {};
    }

    return response.result();
}

op_string RPCClient::CreateRandomTx(size_t size) {
    CreateRandomTxRequest request;
    CreateRandomTxResponse response;

    request.set_size(size);
    if (!ClientCallback([&](auto* context, const auto& request, auto* response)
                            -> grpc::Status { return commander_stub_->CreateRandomTx(context, request, response); },
                        request, &response)) {
        return {};
    }

    return response.result();
}

op_string RPCClient::CreateTx(const std::vector<std::pair<uint64_t, std::string>>& outputs, uint64_t fee) {
    CreateTxRequest request;
    CreateTxResponse response;

    request.set_fee(fee);
    for (auto& output : outputs) {
        auto rpc_output = request.add_outputs();
        rpc_output->set_listing(output.second);
        rpc_output->set_money(output.first);
    }

    if (!ClientCallback([&](auto* context, const auto& request, auto* response)
                            -> grpc::Status { return commander_stub_->CreateTx(context, request, response); },
                        request, &response)) {
        return {};
    }

    return response.txinfo();
}

op_string RPCClient::GetBalance() {
    EmptyRequest request;
    GetBalanceResponse response;

    if (!ClientCallback([&](auto* context, const auto& request, auto* response)
                            -> grpc::Status { return commander_stub_->GetBalance(context, request, response); },
                        request, &response)) {
        return {};
    }

    return response.coin();
}

op_string RPCClient::GenerateNewKey() {
    EmptyRequest request;
    GenerateNewKeyResponse response;

    if (!ClientCallback([&](auto* context, const auto& request, auto* response)
                            -> grpc::Status { return commander_stub_->GenerateNewKey(context, request, response); },
                        request, &response)) {
        return {};
    }

    return response.address();
}

op_string RPCClient::Redeem(const std::string& addr, uint64_t coins) {
    RedeemRequest request;
    RedeemResponse response;

    request.set_address(addr);
    request.set_coins(coins);
    if (!ClientCallback([&](auto* context, const auto& request, auto* response)
                            -> grpc::Status { return commander_stub_->Redeem(context, request, response); },
                        request, &response)) {
        return {};
    }

    return response.result();
}

op_string RPCClient::SetPassphrase(const std::string& passphrase) {
    SetPassphraseRequest request;
    SetPassphraseResponse response;

    request.set_passphrase(passphrase);
    if (!ClientCallback([&](auto* context, const auto& request, auto* response)
                            -> grpc::Status { return commander_stub_->SetPassphrase(context, request, response); },
                        request, &response)) {
        return {};
    }

    return response.responseinfo();
}

op_string RPCClient::ChangePassphrase(const std::string& oldPassphrase, const std::string& newPassphrase) {
    ChangePassphraseRequest request;
    ChangePassphraseResponse response;

    request.set_oldpassphrase(oldPassphrase);
    request.set_newpassphrase(newPassphrase);
    if (!ClientCallback([&](auto* context, const auto& request, auto* response)
                            -> grpc::Status { return commander_stub_->ChangePassphrase(context, request, response); },
                        request, &response)) {
        return {};
    }

    return response.responseinfo();
}

op_string RPCClient::Login(const std::string& passphrase) {
    LoginRequest request;
    LoginResponse response;

    request.set_passphrase(passphrase);
    if (!ClientCallback([&](auto* context, const auto& request, auto* response)
                            -> grpc::Status { return commander_stub_->Login(context, request, response); },
                        request, &response)) {
        return {};
    }

    return response.responseinfo();
}

op_string RPCClient::ConnectPeers(const std::vector<std::string>& addresses) {
    ConnectRequest request;
    ConnectResponse response;

    if (addresses.empty()) {
        std::cout << "Please specify at least one address to connect" << std::endl;
        return {};
    }
    for (const auto& address : addresses) {
        request.add_address(address);
    }

    if (!ClientCallback([&](auto* context, const auto& request, auto* response)
                            -> grpc::Status { return commander_stub_->ConnectPeer(context, request, response); },
                        request, &response)) {
        return {};
    }

    return response.result();
}

op_string RPCClient::DisconnectPeers(const std::vector<std::string>& addresses) {
    DisconnectPeerRequest request;
    DisconnectPeerResponse response;

    for (const auto& address : addresses) {
        request.add_address(address);
    }

    if (!ClientCallback([&](auto* context, const auto& request, auto* response)
                            -> grpc::Status { return commander_stub_->DisconnectPeer(context, request, response); },
                        request, &response)) {
        return {};
    }

    return response.result();
}

op_string RPCClient::DisconnectAllPeers() {
    EmptyRequest request;
    DisconnectAllResponse response;

    if (!ClientCallback([&](auto* context, const auto& request, auto* response)
                            -> grpc::Status { return commander_stub_->DisconnectAllPeers(context, request, response); },
                        request, &response)) {
        return {};
    }

    return response.result();
}

std::optional<bool> RPCClient::SyncCompleted() {
    EmptyRequest request;
    SyncStatusResponse response;

    if (!ClientCallback([&](auto* context, const auto& request, auto* response)
                            -> grpc::Status { return commander_stub_->SyncCompleted(context, request, response); },
                        request, &response)) {
        return {};
    }

    return response.completed();
}

op_string RPCClient::ShowPeer(const std::string& address) {
    ShowPeerRequest request;
    request.set_address(address);
    ShowPeerResponse response;

    return ProcessResponse(
        [&](auto* context, const auto& request, auto* response) -> grpc::Status {
            return commander_stub_->ShowPeer(context, request, response);
        },
        request, &response);
}
