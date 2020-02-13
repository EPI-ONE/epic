// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "rpc_client.h"
#include "return_code.h"
#include "rpc.pb.h"

#include <google/protobuf/util/json_util.h>


using namespace rpc;
using google::protobuf::util::MessageToJsonString;
using op_string = std::optional<std::string>;

const google::protobuf::util::JsonPrintOptions& GetOption() {
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
    EmptyMessage request;
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
    EmptyMessage request;
    GetForksResponse response;
    return ProcessResponse(
        [&](auto* context, const auto& request, auto* response) -> grpc::Status {
            return be_stub_->GetForks(context, request, response);
        },
        request, &response);
}

op_string RPCClient::GetPeerChains() {
    EmptyMessage request;
    GetPeerChainsResponse response;
    return ProcessResponse(
        [&](auto* context, const auto& request, auto* response) -> grpc::Status {
            return be_stub_->GetPeerChains(context, request, response);
        },
        request, &response);
}

op_string RPCClient::GetRecentStat() {
    EmptyMessage request;
    GetRecentStatResponse response;
    return ProcessResponse(
        [&](auto* context, const auto& request, auto* response) -> grpc::Status {
            return be_stub_->GetRecentStat(context, request, response);
        },
        request, &response);
}

op_string RPCClient::Statistic() {
    EmptyMessage request;
    StatisticResponse response;
    return ProcessResponse(
        [&](auto* context, const auto& request, auto* response) -> grpc::Status {
            return be_stub_->Statistic(context, request, response);
        },
        request, &response);
}

op_string RPCClient::Status() {
    EmptyMessage request;
    StatusResponse response;
    return ProcessResponse(
        [&](auto* context, const auto& request, auto* response) -> grpc::Status {
            return commander_stub_->Status(context, request, response);
        },
        request, &response);
}

bool RPCClient::Stop() {
    EmptyMessage request;
    StopResponse response;

    return ClientCallback(
        [&](auto* context, const auto& request, auto* response) -> grpc::Status {
            return commander_stub_->Stop(context, request, response);
        },
        request, &response);
}

std::optional<bool> RPCClient::StartMiner() {
    EmptyMessage request;
    StartMinerResponse response;

    if (!ClientCallback([&](auto* context, const auto& request, auto* response)
                            -> grpc::Status { return commander_stub_->StartMiner(context, request, response); },
                        request, &response)) {
        return {};
    }

    return response.success();
}

op_string RPCClient::StopMiner() {
    EmptyMessage request;
    StopMinerResponse response;

    if (!ClientCallback([&](auto* context, const auto& request, auto* response)
                            -> grpc::Status { return commander_stub_->StopMiner(context, request, response); },
                        request, &response)) {
        return {};
    }

    return GetReturnStr(response.result());
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

    auto result = response.result();
    if (result == RPCReturn::kFirstRegSuc) {
        return GetReturnStr(result) + " with address " + response.addr();
    } else if (result == RPCReturn::kFirstRegExist) {
        return "";
    } else {
        return GetReturnStr(result);
    }
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

    return GetReturnStr(response.result());
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

    auto result = response.result();
    if (result == RPCReturn::kTxWrongAddr || result == RPCReturn::kTxCreatedSuc) {
        return GetReturnStr(result) + ": " + response.txinfo();
    } else {
        return GetReturnStr(result);
    }
}

op_string RPCClient::GetBalance() {
    EmptyMessage request;
    GetBalanceResponse response;

    if (!ClientCallback([&](auto* context, const auto& request, auto* response)
                            -> grpc::Status { return commander_stub_->GetBalance(context, request, response); },
                        request, &response)) {
        return {};
    }

    auto result = response.result();
    if (result == RPCReturn::kGetBalanceSuc) {
        return std::to_string(response.coin());
    } else {
        return GetReturnStr(result);
    }
}

op_string RPCClient::GenerateNewKey() {
    EmptyMessage request;
    GenerateNewKeyResponse response;

    if (!ClientCallback([&](auto* context, const auto& request, auto* response)
                            -> grpc::Status { return commander_stub_->GenerateNewKey(context, request, response); },
                        request, &response)) {
        return {};
    }

    auto result = response.result();
    if (result == RPCReturn::kGenerateKeySuc) {
        return response.address();
    } else {
        return GetReturnStr(result);
    }
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

    auto result = response.result();
    if (result == RPCReturn::kRedeemSuc) {
        return GetReturnStr(result) + " to address " + response.addr();
    }
    return GetReturnStr(response.result());
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

    return GetReturnStr(response.result());
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

    return GetReturnStr(response.result());
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

    return GetReturnStr(response.result());
}

op_string RPCClient::GetWalletAddrs() {
    EmptyMessage request;
    GetWalletAddrsResponse response;

    return "";
}

op_string RPCClient::GetTxout(std::string blkHash, uint32_t txIdx, uint32_t outIdx) {
    GetTxoutRequest request;
    GetTxoutResponse response;

    return "";
}

op_string RPCClient::GetAllTxout() {
    EmptyMessage request;
    GetAllTxoutResponse response;

    return "";
}

std::optional<bool> RPCClient::ValidateAddr(std::string addr) {
    ValidateAddrRequest request;
    BooleanResponse response;

    return false;
}

std::optional<bool> RPCClient::VerifyMessage(std::string input, std::string output) {
    VerifyMessageRequest request;
    BooleanResponse response;

    return false;
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
    EmptyMessage request;
    DisconnectAllResponse response;

    if (!ClientCallback([&](auto* context, const auto& request, auto* response)
                            -> grpc::Status { return commander_stub_->DisconnectAllPeers(context, request, response); },
                        request, &response)) {
        return {};
    }

    return response.result();
}

std::optional<bool> RPCClient::SyncCompleted() {
    EmptyMessage request;
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

op_string RPCClient::Subscribe(const std::string &address, uint8_t sub_type){
    SubscribeRequest request;
    request.set_address(address);
    request.set_sub_type(sub_type);
    SubscribeResponse response;

    return ProcessResponse(
        [&](auto* context, const auto& request, auto* response) -> grpc::Status{
            return commander_stub_->Subscribe(context, request, response);
        },
        request, &response);
}

void RPCClient::DeleteSubscriber(const std::string &address){
    DelSubscriberRequest request;
    request.set_address(address);
    EmptyMessage response;

    ProcessResponse(
        [&](auto* context, const auto& request, auto* response) -> grpc::Status{
            return commander_stub_->DelSubscriber(context, request, response);
        },
        request, &response);
}
