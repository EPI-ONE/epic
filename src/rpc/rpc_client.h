#ifndef __SRC_RPCCLIENT_H__
#define __SRC_RPCCLIENT_H__

#include "spdlog/sinks/basic_file_sink.h"
#include <grpc++/grpc++.h>
#include <rpc.grpc.pb.h>
#include <rpc.pb.h>

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
using rpc::CreateTxRequest;
using rpc::CreateTxResponse;
using rpc::StartMinerRequest;
using rpc::StartMinerResponse;
using rpc::StatusRequest;
using rpc::StatusResponse;
using rpc::StopMinerRequest;
using rpc::StopMinerResponse;
using rpc::StopRequest;
using rpc::StopResponse;

class RPCClient {
public:
    RPCClient(std::shared_ptr<grpc::Channel> channel);

    std::optional<rpc::Block> GetBlock(std::string&);

    std::optional<std::vector<rpc::Block>> GetLevelSet(std::string&);

    std::optional<size_t> GetLevelSetSize(std::string&);

    std::optional<rpc::Block> GetLatestMilestone();

    std::optional<std::vector<rpc::Block>> GetNewMilestoneSince(std::string&, size_t);
    std::optional<StatusResponse> Status();

    bool Stop();

    std::optional<bool> StartMiner();

    std::optional<bool> StopMiner();

    std::optional<std::string> CreateTx(size_t size);

private:
    std::unique_ptr<BasicBlockExplorerRPC::Stub> be_stub_;
    std::unique_ptr<CommanderRPC::Stub> commander_stub_;
};

#endif //__SRC_RPCCLIENT_H__
