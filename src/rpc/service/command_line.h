// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef EPIC_COMMAND_LINE_H
#define EPIC_COMMAND_LINE_H

#include "init.h"
#include "miner.h"
#include "rpc_tools.h"
#include "wallet.h"

class CommanderRPCServiceImpl final : public CommanderRPC::Service {
    grpc::Status Status(grpc::ServerContext* context, const EmptyRequest* request, StatusResponse* reply) override {
        const auto& latestMS = DAG->GetMilestoneHead();
        reply->set_latestmshash(std::to_string(latestMS->cblock->GetHash()));
        reply->set_isminerrunning(MINER->IsRunning());

        return grpc::Status::OK;
    }

    grpc::Status Stop(grpc::ServerContext* context, const EmptyRequest* request, StopResponse* reply) override {
        b_shutdown = true;
        return grpc::Status::OK;
    }

    grpc::Status StartMiner(grpc::ServerContext* context,
                            const EmptyRequest* request,
                            StartMinerResponse* reply) override {
        if (MINER->IsRunning()) {
            reply->set_success(false);
        } else {
            MINER->Stop();
            MINER->Run();
            if (MINER->IsRunning()) {
                reply->set_success(MINER->IsRunning());
            }
        }
        return grpc::Status::OK;
    }

    grpc::Status StopMiner(grpc::ServerContext* context,
                           const EmptyRequest* request,
                           StopMinerResponse* reply) override {
        if (!MINER->IsRunning()) {
            reply->set_result("Miner is not running yet");
        } else if (!MINER->Stop()) {
            reply->set_result("Failed to stop miner");
        } else {
            reply->set_result("Miner is successfully stopped");
        }
        return grpc::Status::OK;
    }

    grpc::Status CreateFirstReg(grpc::ServerContext* context,
                                const CreateFirstRegRequest* request,
                                CreateFirstRegResponse* reply) override {
        if (!WALLET) {
            reply->set_result("Wallet has not been started");
        } else if (!WALLET->IsLoggedIn()) {
            reply->set_result("Please log in or set up a new passphrase");
        } else {
            std::optional<CKeyID> addr{DecodeAddress(request->address())};

            if (addr) {
                std::string encoded_addr;
                if (request->force()) {
                    encoded_addr = WALLET->CreateFirstRegistration(*addr);
                } else {
                    encoded_addr = WALLET->CreateFirstRegWhenPossible(*addr);
                }

                if (!encoded_addr.empty()) {
                    reply->set_result("Successfully created the first registration with address " + encoded_addr);
                }

            } else {
                reply->set_result("Invalid address: " + request->address());
            }
        }

        return grpc::Status::OK;
    }

    grpc::Status CreateRandomTx(grpc::ServerContext* context,
                                const CreateRandomTxRequest* request,
                                CreateRandomTxResponse* reply) override {
        if (!WALLET) {
            reply->set_result("Wallet has not been started");
        } else if (!WALLET->IsLoggedIn()) {
            reply->set_result("Please log in or set up a new passphrase");
        } else {
            WALLET->CreateRandomTx(request->size());
            reply->set_result("Now wallet is creating tx");
        }
        return grpc::Status::OK;
    }

    grpc::Status CreateTx(grpc::ServerContext* context,
                          const CreateTxRequest* request,
                          CreateTxResponse* reply) override {
        std::vector<std::pair<Coin, CKeyID>> outputs;
        if (!WALLET) {
            reply->set_txinfo("Wallet has not been started");
        } else if (!WALLET->IsLoggedIn()) {
            reply->set_txinfo("Please log in or set up a new passphrase");
        } else if (request->outputs().empty()) {
            reply->set_txinfo("Please specify at least one output");
        } else {
            for (auto& output : request->outputs()) {
                Coin coin(output.money());
                auto address = DecodeAddress(output.listing());
                if (!address) {
                    reply->set_txinfo("Wrong address: " + output.listing());
                    return grpc::Status::OK;
                } else {
                    outputs.emplace_back(std::make_pair(coin, *address));
                }
            }
            auto tx = WALLET->CreateTxAndSend(outputs, request->fee());
            if (!tx) {
                reply->set_txinfo("Failed to create tx. Please check if you have enough balance.");
            } else {
                reply->set_txinfo(std::to_string(*tx));
            }
        }
        return grpc::Status::OK;
    }

    grpc::Status GenerateNewKey(grpc::ServerContext* context,
                                const EmptyRequest* request,
                                GenerateNewKeyResponse* reply) override {
        if (!WALLET) {
            reply->set_address("Wallet has not been started");
        } else if (!WALLET->IsLoggedIn()) {
            reply->set_address("Please log in or set up a new passphrase");
        } else {
            auto key = WALLET->CreateNewKey(true);
            reply->set_address(EncodeAddress(key));
        }
        return grpc::Status::OK;
    }

    grpc::Status GetBalance(grpc::ServerContext* context,
                            const EmptyRequest* request,
                            GetBalanceResponse* reply) override {
        if (!WALLET) {
            reply->set_coin("Wallet has not been started");
        } else if (!WALLET->IsLoggedIn()) {
            reply->set_coin("Please log in or set up a new passphrase");
        } else {
            auto balance = WALLET->GetBalance();
            reply->set_coin(std::to_string(balance.GetValue()));
        }
        return grpc::Status::OK;
    }

    grpc::Status SetPassphrase(grpc::ServerContext* context,
                               const SetPassphraseRequest* request,
                               SetPassphraseResponse* reply) override {
        if (!WALLET) {
            reply->set_responseinfo("Wallet has not been started");
        } else if (WALLET->IsCrypted() || WALLET->ExistMasterInfo()) {
            reply->set_responseinfo("Wallet has already be encrypted with a passphrase");
        } else if (!WALLET->SetPassphrase(SecureString{request->passphrase()})) {
            reply->set_responseinfo("Failed to set passphrase");
        } else {
            reply->set_responseinfo("Your passphrase has been successfully set!");
        }
        return grpc::Status::OK;
    }

    grpc::Status ChangePassphrase(grpc::ServerContext* context,
                                  const ChangePassphraseRequest* request,
                                  ChangePassphraseResponse* reply) override {
        if (!WALLET) {
            reply->set_responseinfo("Wallet has not been started");
        } else if (!WALLET->IsCrypted() && !WALLET->ExistMasterInfo()) {
            reply->set_responseinfo("Wallet has no phrase set. Please set one first");
        } else if (!WALLET->ChangePassphrase(SecureString{request->oldpassphrase()},
                                             SecureString{request->newpassphrase()})) {
            reply->set_responseinfo("Failed to change passphrase. Please check passphrase");
        } else {
            reply->set_responseinfo("Your passphrase is successfully updated!");
        }
        return grpc::Status::OK;
    }

    grpc::Status Login(grpc::ServerContext* context, const LoginRequest* request, LoginResponse* reply) override {
        if (!WALLET) {
            reply->set_responseinfo("Wallet has not been started");
        } else if (!WALLET->ExistMasterInfo() && !WALLET->IsCrypted()) {
            reply->set_responseinfo("Wallet has no phrase set. Please set one first");
        } else if (!WALLET->CheckPassphrase(SecureString{request->passphrase()})) {
            reply->set_responseinfo("Failed to login with the passphrase. Please check passphrase");
        } else {
            reply->set_responseinfo("You are already logged in");
            WALLET->RPCLogin();
        }
        return grpc::Status::OK;
    }

    grpc::Status DisconnectPeer(grpc::ServerContext* context,
                                const rpc::DisconnectPeerRequest* request,
                                rpc::DisconnectPeerResponse* response) override {
        if (!PEERMAN) {
            response->set_result("PeerManager has not been start");
        } else {
            for (int i = 0; i < request->address_size(); ++i) {
                if (PEERMAN->DisconnectPeer(request->address(i))) {
                    response->set_result("Disconnected " + request->address(i) + " successfully");
                } else {
                    response->set_result("Failed to disconnect " + request->address(i));
                }
            }
        }
        return grpc::Status::OK;
    }


    grpc::Status DisconnectAllPeers(grpc::ServerContext* context,
                                    const rpc::EmptyRequest* request,
                                    rpc::DisconnectAllResponse* response) override {
        if (!PEERMAN) {
            response->set_result("PeerManager has not been start");
        } else {
            size_t peer_size = PEERMAN->GetFullyConnectedPeerSize();
            PEERMAN->DisconnectAllPeer();
            PEERMAN->ClearPeers();
            response->set_result("Disconnected " + std::to_string(peer_size) + " peers");
        }
        return grpc::Status::OK;
    }

    grpc::Status ConnectPeer(grpc::ServerContext* context,
                             const rpc::ConnectRequest* request,
                             rpc::ConnectResponse* response) override {
        if (!PEERMAN) {
            response->set_result("PeerManager has not been start");
        } else {
            size_t succeess_size = 0;
            std::stringstream ss;

            for (int i = 0; i < request->address_size(); ++i) {
                const auto& address_str = request->address(i);
                auto address            = NetAddress::GetByIP(address_str);
                ss << "Index " << i << ": " << address_str;
                if (!address) {
                    ss << " is of invalid format, please check it" << '\n';
                    continue;
                }
                if (PEERMAN->ConnectTo(*address)) {
                    ss << " has been connected successfully" << '\n';
                    succeess_size++;
                } else {
                    ss << " failed to be connected" << '\n';
                }
            }
            ss << '\n' << "Trying to connect " << succeess_size << " peers" << '\n';
            response->set_result(ss.str());
        }
        return grpc::Status::OK;
    }

    grpc::Status SyncCompleted(grpc::ServerContext* context,
                               const rpc::EmptyRequest* request,
                               rpc::SyncStatusResponse* response) override {
        if (PEERMAN) {
            response->set_completed(PEERMAN->InitialSyncCompleted());
        } else {
            response->set_completed(false);
        }
        return grpc::Status::OK;
    }

    void AddPeer(rpc::ShowPeerResponse* response, PeerPtr peer) {
        auto rpc_peer = response->add_peer();
        rpc_peer->set_id(peer->peer_id);
        rpc_peer->set_socket(peer->address.ToString());
        rpc_peer->set_valid(peer->IsVaild());
        rpc_peer->set_inbound(peer->IsInbound());
        rpc_peer->set_isfullyconnected(peer->isFullyConnected);
        rpc_peer->set_issyncavailable(peer->isSyncAvailable);
        rpc_peer->set_connected_time(peer->connected_time);
        if (peer->versionMessage) {
            rpc_peer->set_block_version(peer->versionMessage->client_version);
            rpc_peer->set_local_service(peer->versionMessage->local_service);
            rpc_peer->set_app_version(peer->versionMessage->version_info);
        }
    }

    grpc::Status ShowPeer(grpc::ServerContext* context,
                          const rpc::ShowPeerRequest* request,
                          rpc::ShowPeerResponse* response) override {
        auto address = request->address();
        std::transform(address.begin(), address.end(), address.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        if (address == "all") {
            auto peers = PEERMAN->GetAllPeer();
            for (auto& peer : peers) {
                AddPeer(response, peer);
            }
        } else {
            auto peer = PEERMAN->GetPeer(address);
            if (peer) {
                AddPeer(response, peer);
            }
        }

        return grpc::Status::OK;
    }
};

#endif // EPIC_COMMAND_LINE_H
