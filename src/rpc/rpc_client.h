// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef EPIC_RPC_CLIENT_H
#define EPIC_RPC_CLIENT_H

#include "rpc_header.h"

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

    option_string SetPassphrase(const std::string& passphrase);
    option_string ChangePassphrase(const std::string& oldPassphrase, const std::string& newPassphrase);
    option_string Login(const std::string& passphrase);

private:
    std::unique_ptr<BasicBlockExplorerRPC::Stub> be_stub_;
    std::unique_ptr<CommanderRPC::Stub> commander_stub_;
};

#endif // EPIC_RPC_CLIENT_H
