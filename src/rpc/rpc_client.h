#ifndef __SRC_RPCCLIENT_H__
#define __SRC_RPCCLIENT_H__

#include "rpc_header.h"
#include "spdlog/sinks/basic_file_sink.h"


class RPCClient {
    using option_string = std::optional<std::string>;

public:
    RPCClient(std::shared_ptr<grpc::Channel> channel);

    std::optional<rpc::Block> GetBlock(std::string);

    std::optional<std::vector<rpc::Block>> GetLevelSet(std::string);

    std::optional<size_t> GetLevelSetSize(std::string);

    std::optional<rpc::Block> GetLatestMilestone();

    std::optional<std::vector<rpc::Block>> GetNewMilestoneSince(std::string, size_t);
    std::optional<StatusResponse> Status();

    bool Stop();

    std::optional<bool> StartMiner();

    std::optional<bool> StopMiner();

    option_string CreateRandomTx(size_t size);

    option_string CreateTx(const std::vector<std::pair<uint64_t, std::string>>& outpus, uint64_t fee);

    option_string GetBalance();

    option_string GenerateNewKey();


private:
    std::unique_ptr<BasicBlockExplorerRPC::Stub> be_stub_;
    std::unique_ptr<CommanderRPC::Stub> commander_stub_;
};

#endif //__SRC_RPCCLIENT_H__
