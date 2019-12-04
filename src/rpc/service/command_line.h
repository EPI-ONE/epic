// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef EPIC_COMMAND_LINE_H
#define EPIC_COMMAND_LINE_H

#include <grpc++/grpc++.h>
#include <memory>
#include <rpc.grpc.pb.h>
#include <rpc.pb.h>

class Peer;

class CommanderRPCServiceImpl final : public rpc::CommanderRPC::Service {
    grpc::Status Status(grpc::ServerContext* context,
                        const rpc::EmptyRequest* request,
                        rpc::StatusResponse* reply) override;

    grpc::Status Stop(grpc::ServerContext* context,
                      const rpc::EmptyRequest* request,
                      rpc::StopResponse* reply) override;

    grpc::Status StartMiner(grpc::ServerContext* context,
                            const rpc::EmptyRequest* request,
                            rpc::StartMinerResponse* reply) override;

    grpc::Status StopMiner(grpc::ServerContext* context,
                           const rpc::EmptyRequest* request,
                           rpc::StopMinerResponse* reply) override;

    grpc::Status CreateFirstReg(grpc::ServerContext* context,
                                const rpc::CreateFirstRegRequest* request,
                                rpc::CreateFirstRegResponse* reply) override;

    grpc::Status Redeem(grpc::ServerContext* context,
                        const rpc::RedeemRequest* request,
                        rpc::RedeemResponse* reply) override;

    grpc::Status CreateRandomTx(grpc::ServerContext* context,
                                const rpc::CreateRandomTxRequest* request,
                                rpc::CreateRandomTxResponse* reply) override;

    grpc::Status CreateTx(grpc::ServerContext* context,
                          const rpc::CreateTxRequest* request,
                          rpc::CreateTxResponse* reply) override;

    grpc::Status GenerateNewKey(grpc::ServerContext* context,
                                const rpc::EmptyRequest* request,
                                rpc::GenerateNewKeyResponse* reply) override;

    grpc::Status GetBalance(grpc::ServerContext* context,
                            const rpc::EmptyRequest* request,
                            rpc::GetBalanceResponse* reply) override;

    grpc::Status SetPassphrase(grpc::ServerContext* context,
                               const rpc::SetPassphraseRequest* request,
                               rpc::SetPassphraseResponse* reply) override;

    grpc::Status ChangePassphrase(grpc::ServerContext* context,
                                  const rpc::ChangePassphraseRequest* request,
                                  rpc::ChangePassphraseResponse* reply) override;

    grpc::Status Login(grpc::ServerContext* context,
                       const rpc::LoginRequest* request,
                       rpc::LoginResponse* reply) override;

    grpc::Status DisconnectPeer(grpc::ServerContext* context,
                                const rpc::DisconnectPeerRequest* request,
                                rpc::DisconnectPeerResponse* response) override;

    grpc::Status DisconnectAllPeers(grpc::ServerContext* context,
                                    const rpc::EmptyRequest* request,
                                    rpc::DisconnectAllResponse* response) override;

    grpc::Status ConnectPeer(grpc::ServerContext* context,
                             const rpc::ConnectRequest* request,
                             rpc::ConnectResponse* response) override;

    grpc::Status SyncCompleted(grpc::ServerContext* context,
                               const rpc::EmptyRequest* request,
                               rpc::SyncStatusResponse* response) override;

    grpc::Status ShowPeer(grpc::ServerContext* context,
                          const rpc::ShowPeerRequest* request,
                          rpc::ShowPeerResponse* response) override;

private:
    void AddPeer(rpc::ShowPeerResponse* response, std::shared_ptr<Peer> peer);
};

#endif // EPIC_COMMAND_LINE_H
