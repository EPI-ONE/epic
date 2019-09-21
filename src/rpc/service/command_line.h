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
    grpc::Status Status(grpc::ServerContext* context, const StatusRequest* request, StatusResponse* reply) override {
        auto latestMS     = DAG->GetMilestoneHead();
        auto latestMSHash = ToRPCHash(latestMS->cblock->GetHash());
        reply->set_allocated_latestmshash(latestMSHash);
        reply->set_isminerrunning(MINER->IsRunning());

        return grpc::Status::OK;
    }

    grpc::Status Stop(grpc::ServerContext* context, const StopRequest* request, StopResponse* reply) override {
        b_shutdown = true;
        return grpc::Status::OK;
    }

    grpc::Status StartMiner(grpc::ServerContext* context,
                            const StartMinerRequest* request,
                            StartMinerResponse* reply) override {
        if (MINER->IsRunning()) {
            reply->set_success(false);
        } else {
            MINER->Run();
            reply->set_success(MINER->IsRunning());
        }
        return grpc::Status::OK;
    }

    grpc::Status StopMiner(grpc::ServerContext* context,
                           const StopMinerRequest* request,
                           StopMinerResponse* reply) override {
        if (MINER->IsRunning()) {
            MINER->Stop();
            reply->set_success(true);
        } else {
            reply->set_success(false);
        }
        return grpc::Status::OK;
    }

    grpc::Status CreateRandomTx(grpc::ServerContext* context,
                                const CreateRandomTxRequest* request,
                                CreateRandomTxResponse* reply) override {
        if (!WALLET) {
            reply->set_result("Wallet has not been started");
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
                reply->set_txinfo("Failed to create tx. Please check if you have enough balance.");
            } else {
                reply->set_txinfo(std::to_string(*tx));
            }
        }
        return grpc::Status::OK;
    }

    grpc::Status GenerateNewKey(grpc::ServerContext* context,
                                const GenerateNewKeyRequest* request,
                                GenerateNewKeyResponse* reply) override {
        if (!WALLET) {
            reply->set_address("Wallet has not been started");
        } else {
            auto key = WALLET->CreateNewKey(true);
            reply->set_address(EncodeAddress(key));
        }
        return grpc::Status::OK;
    }

    grpc::Status GetBalance(grpc::ServerContext* context,
                            const GetBalanceRequest* request,
                            GetBalanceResponse* reply) override {
        if (!WALLET) {
            reply->set_coin("Wallet has not been started");
        } else {
            auto balance = WALLET->GetBalance();
            reply->set_coin(std::to_string(balance.GetValue()));
        }
        return grpc::Status::OK;
    }
};


#endif // EPIC_COMMAND_LINE_H
