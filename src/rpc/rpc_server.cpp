// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "rpc_server.h"

grpc::Status BasicBlockExplorerRPCServiceImpl::GetBlock(grpc::ServerContext* context,
                                                        const GetBlockRequest* request,
                                                        GetBlockResponse* reply) {
    auto vertex = DAG->GetMainChainVertex(ToHash(request->hash()));
    if (!vertex) {
        return grpc::Status::OK;
    }
    rpc::Block* b = ToRPCBlock(*(vertex->cblock));
    reply->set_allocated_block(b);
    return grpc::Status::OK;
}

grpc::Status BasicBlockExplorerRPCServiceImpl::GetLevelSet(grpc::ServerContext* context,
                                                           const GetLevelSetRequest* request,
                                                           GetLevelSetResponse* reply) {
    auto ls = DAG->GetMainChainLevelSet(ToHash(request->hash()));
    if (ls.empty()) {
        return grpc::Status::OK;
    }
    for (auto localBlock : ls) {
        auto newBlock   = reply->add_blocks();
        auto blockValue = ToRPCBlock(*localBlock);
        *newBlock       = *blockValue;
        delete blockValue;
    }
    return grpc::Status::OK;
}

grpc::Status BasicBlockExplorerRPCServiceImpl::GetLevelSetSize(grpc::ServerContext* context,
                                                               const GetLevelSetSizeRequest* request,
                                                               GetLevelSetSizeResponse* reply) {
    auto ls = DAG->GetMainChainLevelSet(ToHash(request->hash()));
    reply->set_size(ls.size());
    return grpc::Status::OK;
}

grpc::Status BasicBlockExplorerRPCServiceImpl::GetNewMilestoneSince(grpc::ServerContext* context,
                                                                    const GetNewMilestoneSinceRequest* request,
                                                                    GetNewMilestoneSinceResponse* reply) {
    auto vertex = DAG->GetState(ToHash(request->hash()));
    if (!vertex) {
        return grpc::Status::OK;
    }
    auto milestone_hashes = DAG->TraverseMilestoneForward(*vertex, request->number());
    for (size_t i = 0; i < milestone_hashes.size(); ++i) {
        auto vtx        = DAG->GetState(milestone_hashes[i]);
        auto newBlock   = reply->add_blocks();
        auto blockValue = ToRPCBlock(*(vtx->cblock));
        *newBlock       = *blockValue;
        delete blockValue;
    }
    return grpc::Status::OK;
}

grpc::Status BasicBlockExplorerRPCServiceImpl::GetLatestMilestone(grpc::ServerContext* context,
                                                                  const GetLatestMilestoneRequest* request,
                                                                  GetLatestMilestoneResponse* reply) {
    auto vertex   = DAG->GetMilestoneHead();
    rpc::Block* b = ToRPCBlock(*(vertex->cblock));
    reply->set_allocated_milestone(b);
    return grpc::Status::OK;
}

grpc::Status CommanderRPCServiceImpl::Status(grpc::ServerContext* context,
                                             const StatusRequest* request,
                                             StatusResponse* reply) {
    auto latestMS     = DAG->GetMilestoneHead();
    auto latestMSHash = ToRPCHash(latestMS->cblock->GetHash());
    reply->set_allocated_latestmshash(latestMSHash);
    reply->set_isminerrunning(MINER->IsRunning());

    return grpc::Status::OK;
}

grpc::Status CommanderRPCServiceImpl::Stop(grpc::ServerContext* context,
                                           const StopRequest* request,
                                           StopResponse* reply) {
    b_shutdown = true;
    return grpc::Status::OK;
}

grpc::Status CommanderRPCServiceImpl::StartMiner(grpc::ServerContext* context,
                                                 const StartMinerRequest* request,
                                                 StartMinerResponse* reply) {
    if (MINER->IsRunning()) {
        reply->set_success(false);
    } else {
        MINER->Run();
        reply->set_success(true);
    }
    return grpc::Status::OK;
}

grpc::Status CommanderRPCServiceImpl::StopMiner(grpc::ServerContext* context,
                                                const StopMinerRequest* request,
                                                StopMinerResponse* reply) {
    if (MINER->IsRunning()) {
        MINER->Stop();
        reply->set_success(true);
    } else {
        reply->set_success(false);
    }
    return grpc::Status::OK;
}

grpc::Status CommanderRPCServiceImpl::CreateRandomTx(grpc::ServerContext* context,
                                                     const CreateRandomTxRequest* request,
                                                     CreateRandomTxResponse* reply) {
    if (!WALLET) {
        reply->set_result("Wallet has not been started");
    } else {
        WALLET->CreateRandomTx(request->size());
        reply->set_result("Now wallet is creating tx");
    }
    return grpc::Status::OK;
}

grpc::Status CommanderRPCServiceImpl::CreateTx(grpc::ServerContext* context,
                                               const CreateTxRequest* request,
                                               CreateTxResponse* reply) {
    std::vector<std::pair<Coin, CKeyID>> outputs;
    if (!WALLET) {
        reply->set_txinfo("Wallet has not been started");
    } else if (request->outputs().empty()) {
        reply->set_txinfo("Please specify at least one output");
    } else {
        for (auto& output : request->outputs()) {
            Coin coin(output.money());
            auto address = DecodeAddress(output.address());
            if (!address) {
                reply->set_txinfo("Wrong address: " + output.address());
                return grpc::Status::OK;
            } else {
                outputs.emplace_back(std::make_pair(coin, *address));
            }
        }
        auto tx = WALLET->CreateTxAndSend(outputs, request->fee());
        if (!tx) {
            reply->set_txinfo("failed to create tx, please check if you have enough balance");
        } else {
            reply->set_txinfo(std::to_string(*tx));
        }
    }
    return grpc::Status::OK;
}

grpc::Status CommanderRPCServiceImpl::GenerateNewKey(grpc::ServerContext* context,
                                                     const GenerateNewKeyRequest* request,
                                                     GenerateNewKeyResponse* reply) {
    if (!WALLET) {
        reply->set_address("Wallet has not been started");
    } else {
        auto key     = WALLET->CreateNewKey(false);
        auto pubkey  = key.GetPubKey();
        auto address = pubkey.GetID();
        reply->set_address(EncodeAddress(address));
        reply->set_privatekey(EncodeSecret(key));
    }
    return grpc::Status::OK;
}

grpc::Status CommanderRPCServiceImpl::GetBalance(grpc::ServerContext* context,
                                                 const GetBalanceRequest* request,
                                                 GetBalanceResponse* reply) {
    if (!WALLET) {
        reply->set_coin("Wallet has not been started");
    } else {
        auto balance = WALLET->GetBalance();
        reply->set_coin(std::to_string(balance.GetValue()));
    }
    return grpc::Status::OK;
}

RPCServer::RPCServer(NetAddress adr) {
    this->server_address_ = adr.ToString();
}

void RPCServer::Start() {
    std::thread t(&RPCServer::LaunchServer, this);
    t.detach();
}

void RPCServer::Shutdown() {
    isRunning_ = false;
}

bool RPCServer::IsRunning() {
    return isRunning_.load();
}

void RPCServer::LaunchServer() {
    BasicBlockExplorerRPCServiceImpl be_service;
    CommanderRPCServiceImpl commander_service;
    grpc::ServerBuilder builder;
    builder.AddListeningPort(this->server_address_, grpc::InsecureServerCredentials());
    builder.RegisterService(&be_service);
    builder.RegisterService(&commander_service);
    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
    isRunning_ = true;
    spdlog::info("RPC Server is running on {}", this->server_address_);
    while (IsRunning()) {
        std::this_thread::yield();
    }
    spdlog::info("RPC Server is shutting down");
    server->Shutdown();
}
