// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "command_line.h"
#include "init.h"
#include "miner.h"
#include "return_code.h"
#include "rpc.pb.h"
#include "rpc_tools.h"
#include "subscription.h"
#include "wallet.h"

using namespace rpc;

grpc::Status CommanderRPCServiceImpl::Status(grpc::ServerContext* context,
                                             const EmptyMessage* request,
                                             StatusResponse* reply) {
    const auto& latestMS = DAG->GetMilestoneHead();
    reply->set_latestmshash(std::to_string(latestMS->cblock->GetHash()));
    reply->set_isminerrunning(MINER->IsRunning());

    return grpc::Status::OK;
}

grpc::Status CommanderRPCServiceImpl::Stop(grpc::ServerContext* context,
                                           const EmptyMessage* request,
                                           StopResponse* reply) {
    b_shutdown = true;
    return grpc::Status::OK;
}

grpc::Status CommanderRPCServiceImpl::StartMiner(grpc::ServerContext* context,
                                                 const EmptyMessage* request,
                                                 StartMinerResponse* reply) {
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

grpc::Status CommanderRPCServiceImpl::StopMiner(grpc::ServerContext* context,
                                                const EmptyMessage* request,
                                                StopMinerResponse* reply) {
    if (!MINER->IsRunning()) {
        reply->set_result(RPCReturn::kMinerNotRunning);
    } else if (!MINER->Stop()) {
        reply->set_result(RPCReturn::kMinerStopFailed);
    } else {
        reply->set_result(RPCReturn::kMinerStop);
    }
    return grpc::Status::OK;
}

grpc::Status CommanderRPCServiceImpl::CreateFirstReg(grpc::ServerContext* context,
                                                     const CreateFirstRegRequest* request,
                                                     CreateFirstRegResponse* reply) {
    if (!WALLET) {
        reply->set_result(RPCReturn::kWalletNotStarted);
    } else if (!WALLET->IsLoggedIn()) {
        reply->set_result(RPCReturn::kWalletNotLoggedIn);
    } else {
        CKeyID addr;
        if (request->address().empty()) {
            addr = WALLET->CreateNewKey(true);
        } else {
            if (auto decoded = DecodeAddress(request->address())) {
                addr = *decoded;
            } else {
                reply->set_result(RPCReturn::kFirstRegInvalid);
                return grpc::Status::OK;
            }
        }

        std::string encoded_addr;
        if (request->force()) {
            encoded_addr = WALLET->CreateFirstRegistration(addr);
        } else {
            encoded_addr = WALLET->CreateFirstRegWhenPossible(addr);
        }

        if (!encoded_addr.empty()) {
            reply->set_result(RPCReturn::kFirstRegSuc);
            reply->set_addr(encoded_addr);
        } else {
            reply->set_result(RPCReturn::kFirstRegExist);
        }
    }

    return grpc::Status::OK;
}

grpc::Status CommanderRPCServiceImpl::Redeem(grpc::ServerContext* context,
                                             const rpc::RedeemRequest* request,
                                             rpc::RedeemResponse* reply) {
    if (!WALLET) {
        reply->set_result(RPCReturn::kWalletNotStarted);
    } else if (!WALLET->IsLoggedIn()) {
        reply->set_result(RPCReturn::kWalletNotLoggedIn);
    } else if (!WALLET->Redeemable(request->coins())) {
        reply->set_result(RPCReturn::kRedeemExceed);
    } else if (WALLET->HasPendingRedemption()) {
        reply->set_result(RPCReturn::kRedeemPending);
    } else {
        CKeyID addr;
        if (request->address().empty()) {
            addr = WALLET->CreateNewKey(true);
        } else {
            if (auto decoded = DecodeAddress(request->address())) {
                addr = *decoded;
            } else {
                reply->set_result(RPCReturn::kTxWrongAddr);
                return grpc::Status::OK;
            }
        }

        auto last_redem = WALLET->CreateRedemption(addr, request->coins());

        reply->set_result(RPCReturn::kRedeemSuc);
        reply->set_addr(last_redem);
    }

    return grpc::Status::OK;
}

grpc::Status CommanderRPCServiceImpl::CreateRandomTx(grpc::ServerContext* context,
                                                     const CreateRandomTxRequest* request,
                                                     CreateRandomTxResponse* reply) {
    if (!WALLET) {
        reply->set_result(RPCReturn::kWalletNotStarted);
    } else if (!WALLET->IsLoggedIn()) {
        reply->set_result(RPCReturn::kWalletNotLoggedIn);
    } else {
        WALLET->CreateRandomTx(request->size());
        reply->set_result(RPCReturn::kTxCreatedSuc);
    }
    return grpc::Status::OK;
}

grpc::Status CommanderRPCServiceImpl::CreateTx(grpc::ServerContext* context,
                                               const CreateTxRequest* request,
                                               CreateTxResponse* reply) {
    std::vector<std::pair<Coin, CKeyID>> outputs;
    if (!WALLET) {
        reply->set_result(RPCReturn::kWalletNotStarted);
    } else if (!WALLET->IsLoggedIn()) {
        reply->set_result(RPCReturn::kWalletNotLoggedIn);
    } else if (request->outputs().empty()) {
        reply->set_result(RPCReturn::kTxNoOutput);
    } else {
        for (auto& output : request->outputs()) {
            Coin coin(output.money());
            auto address = DecodeAddress(output.listing());
            if (!address) {
                reply->set_result(RPCReturn::kTxWrongAddr);
                reply->set_txinfo(output.listing());
                return grpc::Status::OK;
            } else {
                outputs.emplace_back(std::make_pair(coin, *address));
            }
        }
        auto tx = WALLET->CreateTxAndSend(outputs, request->fee());
        if (!tx) {
            reply->set_result(RPCReturn::kTxCreateTxFailed);
        } else {
            reply->set_result(RPCReturn::kTxCreatedSuc);
            reply->set_txinfo(std::to_string(*tx));
        }
    }
    return grpc::Status::OK;
}

grpc::Status CommanderRPCServiceImpl::GenerateNewKey(grpc::ServerContext* context,
                                                     const EmptyMessage* request,
                                                     GenerateNewKeyResponse* reply) {
    if (!WALLET) {
        reply->set_result(RPCReturn::kWalletNotStarted);
    } else if (!WALLET->IsLoggedIn()) {
        reply->set_result(RPCReturn::kWalletNotLoggedIn);
    } else {
        auto key = WALLET->CreateNewKey(true);
        reply->set_result(RPCReturn::kGenerateKeySuc);
        reply->set_address(EncodeAddress(key));
    }
    return grpc::Status::OK;
}

grpc::Status CommanderRPCServiceImpl::GetBalance(grpc::ServerContext* context,
                                                 const EmptyMessage* request,
                                                 GetBalanceResponse* reply) {
    if (!WALLET) {
        reply->set_result(RPCReturn::kWalletNotStarted);
    } else if (!WALLET->IsLoggedIn()) {
        reply->set_result(RPCReturn::kWalletNotLoggedIn);
    } else {
        auto balance = WALLET->GetBalance();
        reply->set_result(RPCReturn::kGetBalanceSuc);
        reply->set_coin(balance.GetValue());
    }
    return grpc::Status::OK;
}

grpc::Status CommanderRPCServiceImpl::SetPassphrase(grpc::ServerContext* context,
                                                    const SetPassphraseRequest* request,
                                                    SetPassphraseResponse* reply) {
    if (!WALLET) {
        reply->set_result(RPCReturn::kWalletNotStarted);
    } else if (WALLET->IsCrypted() || WALLET->ExistMasterInfo()) {
        reply->set_result(RPCReturn::kWalletEncrypted);
    } else if (!WALLET->SetPassphrase(SecureString{request->passphrase()})) {
        reply->set_result(RPCReturn::kWalletPhraseSetFailed);
    } else {
        reply->set_result(RPCReturn::kWalletPhraseSet);
    }
    return grpc::Status::OK;
}

grpc::Status CommanderRPCServiceImpl::ChangePassphrase(grpc::ServerContext* context,
                                                       const ChangePassphraseRequest* request,
                                                       ChangePassphraseResponse* reply) {
    if (!WALLET) {
        reply->set_result(RPCReturn::kWalletNotStarted);
    } else if (!WALLET->IsCrypted() && !WALLET->ExistMasterInfo()) {
        reply->set_result(RPCReturn::kWalletNoPhrase);
    } else if (!WALLET->ChangePassphrase(SecureString{request->oldpassphrase()},
                                         SecureString{request->newpassphrase()})) {
        reply->set_result(RPCReturn::kWalletPhraseChangeFailed);
    } else {
        reply->set_result(RPCReturn::kWalletPhraseUpdated);
    }
    return grpc::Status::OK;
}

grpc::Status CommanderRPCServiceImpl::Login(grpc::ServerContext* context,
                                            const LoginRequest* request,
                                            LoginResponse* reply) {
    if (!WALLET) {
        reply->set_result(RPCReturn::kWalletNotStarted);
    } else if (!WALLET->ExistMasterInfo() && !WALLET->IsCrypted()) {
        reply->set_result(RPCReturn::kWalletNoPhrase);
    } else if (!WALLET->CheckPassphrase(SecureString{request->passphrase()})) {
        reply->set_result(RPCReturn::kWalletLoginFailed);
    } else {
        reply->set_result(RPCReturn::kWalletLoggedIn);
        WALLET->RPCLogin();
    }
    return grpc::Status::OK;
}

grpc::Status CommanderRPCServiceImpl::DisconnectPeer(grpc::ServerContext* context,
                                                     const rpc::DisconnectPeerRequest* request,
                                                     rpc::DisconnectPeerResponse* response) {
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

grpc::Status CommanderRPCServiceImpl::DisconnectAllPeers(grpc::ServerContext* context,
                                                         const rpc::EmptyMessage* request,
                                                         rpc::DisconnectAllResponse* response) {
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

grpc::Status CommanderRPCServiceImpl::ConnectPeer(grpc::ServerContext* context,
                                                  const rpc::ConnectRequest* request,
                                                  rpc::ConnectResponse* response) {
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

grpc::Status CommanderRPCServiceImpl::SyncCompleted(grpc::ServerContext* context,
                                                    const rpc::EmptyMessage* request,
                                                    rpc::SyncStatusResponse* response) {
    if (PEERMAN) {
        response->set_completed(PEERMAN->InitialSyncCompleted());
    } else {
        response->set_completed(false);
    }
    return grpc::Status::OK;
}

void CommanderRPCServiceImpl::AddPeer(rpc::ShowPeerResponse* response, PeerPtr peer) {
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

grpc::Status CommanderRPCServiceImpl::ShowPeer(grpc::ServerContext* context,
                                               const rpc::ShowPeerRequest* request,
                                               rpc::ShowPeerResponse* response) {
    auto address = request->address();
    std::transform(address.begin(), address.end(), address.begin(), [](unsigned char c) { return std::tolower(c); });
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

grpc::Status CommanderRPCServiceImpl::Subscribe(grpc::ServerContext* context,
                                                const rpc::SubscribeRequest* request,
                                                rpc::SubscribeResponse* response) {
    auto address     = request->address();
    uint8_t sub_type = (uint8_t) request->sub_type();
    if (!PUBLISHER) {
        response->set_result("Publisher hasn't been started");
    } else if (PUBLISHER->AddNewSubscriber(address, sub_type)) {
        response->set_result("Success");
    } else {
        response->set_result("Failed to subscribe");
    }

    return grpc::Status::OK;
}

grpc::Status CommanderRPCServiceImpl::DelSubscriber(grpc::ServerContext* context,
                                                    const rpc::DelSubscriberRequest* request,
                                                    rpc::EmptyMessage* response) {
    auto address = request->address();
    if (PUBLISHER) {
        PUBLISHER->DeleteSubscriber(address);
    }

    return grpc::Status::OK;
}

grpc::Status CommanderRPCServiceImpl::NetStat(grpc::ServerContext* context,
                                              const rpc::EmptyMessage* request,
                                              rpc::NetStatResponse* response) {
    response->set_receive_bytes(PEERMAN->GetNetStat().receive_bytes);
    response->set_receive_pkgs(PEERMAN->GetNetStat().receive_packages);
    response->set_send_bytes(PEERMAN->GetNetStat().send_bytes);
    response->set_send_pkgs(PEERMAN->GetNetStat().send_packages);
    response->set_crc_error_bytes(PEERMAN->GetNetStat().crc_error_bytes);
    response->set_crc_error_pkgs(PEERMAN->GetNetStat().crc_error_packages);
    response->set_header_error_pkgs(PEERMAN->GetNetStat().header_error_packages);
    response->set_receive_rate(PEERMAN->GetNetStat().receive_rate);
    response->set_receive_pps(PEERMAN->GetNetStat().receive_pps);
    response->set_send_rate(PEERMAN->GetNetStat().send_rate);
    response->set_send_pps(PEERMAN->GetNetStat().send_pps);
    return grpc::Status::OK;
}
