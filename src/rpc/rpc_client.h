// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef EPIC_RPC_CLIENT_H
#define EPIC_RPC_CLIENT_H

#include <grpc++/grpc++.h>
#include <optional>
#include <rpc.grpc.pb.h>
#include <rpc.pb.h>

class RPCClient {
public:
    explicit RPCClient(std::shared_ptr<grpc::Channel> channel);

    std::optional<std::string> GetBlock(std::string);
    std::optional<std::string> GetLevelSet(std::string);
    std::optional<std::string> GetLevelSetSize(std::string);
    std::optional<std::string> GetLatestMilestone();
    std::optional<std::string> GetNewMilestoneSince(std::string, size_t);
    std::optional<std::string> GetVertex(std::string);
    std::optional<std::string> GetMilestone(std::string);
    std::optional<std::string> GetForks();
    std::optional<std::string> GetPeerChains();
    std::optional<std::string> GetRecentStat();
    std::optional<std::string> Statistic();

    std::optional<std::string> Status();

    bool Stop();

    std::optional<bool> StartMiner();
    std::optional<std::string> StopMiner();

    std::optional<std::string> CreateFirstReg(std::string addr, bool force = false);
    std::optional<std::string> Redeem(const std::string& addr, uint64_t coins);
    std::optional<std::string> CreateRandomTx(size_t size);
    std::optional<std::string> CreateTx(const std::vector<std::pair<uint64_t, std::string>>& outputs, uint64_t fee);
    std::optional<std::string> GenerateNewKey();

    std::optional<std::string> SetPassphrase(const std::string& passphrase);
    std::optional<std::string> ChangePassphrase(const std::string& oldPassphrase, const std::string& newPassphrase);
    std::optional<std::string> Login(const std::string& passphrase);

    std::optional<std::string> GetBalance();
    std::optional<std::string> GetWalletAddrs();
    std::optional<std::string> GetTxout(std::string blkHash, uint32_t txIdx, uint32_t outIdx);
    std::optional<std::string> GetAllTxout();
    std::optional<bool> ValidateAddr(std::string addr);
    std::optional<bool> VerifyMessage(std::string input, std::string output, std::vector<uint8_t> ops);

    std::optional<std::string> DisconnectPeers(const std::vector<std::string>& addresses);
    std::optional<std::string> DisconnectAllPeers();
    std::optional<std::string> ConnectPeers(const std::vector<std::string>& addresses);
    std::optional<bool> SyncCompleted();
    std::optional<std::string> ShowPeer(const std::string&);

    std::optional<std::string> Subscribe(const std::string& address, uint8_t sub_type);
    void DeleteSubscriber(const std::string& address);

private:
    std::unique_ptr<rpc::BasicBlockExplorerRPC::Stub> be_stub_;
    std::unique_ptr<rpc::CommanderRPC::Stub> commander_stub_;
};

#endif // EPIC_RPC_CLIENT_H
