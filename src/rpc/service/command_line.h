// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef EPIC_COMMAND_LINE_H
#define EPIC_COMMAND_LINE_H

#include "rpc_service.h"
#include <memory>

class Peer;

class CommanderRPCServiceImpl final : public CommanderRPC::Service {
    grpc::Status Status(grpc::ServerContext* context, const EmptyRequest* request, StatusResponse* reply) override;

    grpc::Status Stop(grpc::ServerContext* context, const EmptyRequest* request, StopResponse* reply) override;

    grpc::Status StartMiner(grpc::ServerContext* context,
                            const EmptyRequest* request,
                            StartMinerResponse* reply) override;

    grpc::Status StopMiner(grpc::ServerContext* context,
                           const EmptyRequest* request,
                           StopMinerResponse* reply) override;

    grpc::Status CreateFirstReg(grpc::ServerContext* context,
                                const CreateFirstRegRequest* request,
                                CreateFirstRegResponse* reply) override;

    grpc::Status CreateRandomTx(grpc::ServerContext* context,
                                const CreateRandomTxRequest* request,
                                CreateRandomTxResponse* reply) override;

    grpc::Status CreateTx(grpc::ServerContext* context,
                          const CreateTxRequest* request,
                          CreateTxResponse* reply) override;

    grpc::Status GenerateNewKey(grpc::ServerContext* context,
                                const EmptyRequest* request,
                                GenerateNewKeyResponse* reply) override;

    grpc::Status GetBalance(grpc::ServerContext* context,
                            const EmptyRequest* request,
                            GetBalanceResponse* reply) override;

    grpc::Status SetPassphrase(grpc::ServerContext* context,
                               const SetPassphraseRequest* request,
                               SetPassphraseResponse* reply) override;

    grpc::Status ChangePassphrase(grpc::ServerContext* context,
                                  const ChangePassphraseRequest* request,
                                  ChangePassphraseResponse* reply) override;

    grpc::Status Login(grpc::ServerContext* context, const LoginRequest* request, LoginResponse* reply) override;

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

    void AddPeer(rpc::ShowPeerResponse* response, std::shared_ptr<Peer> peer);

    grpc::Status ShowPeer(grpc::ServerContext* context,
                          const rpc::ShowPeerRequest* request,
                          rpc::ShowPeerResponse* response) override;
};

#endif // EPIC_COMMAND_LINE_H
