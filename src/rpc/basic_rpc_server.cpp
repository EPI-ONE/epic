// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "basic_rpc_server.h"
#include "spdlog.h"

#include <thread>

BasicRPCServer::BasicRPCServer(const std::string& addr) : isRunning_{false} {
    this->server_address_ = addr;
}

void BasicRPCServer::Start(std::vector<grpc::Service*> services) {
    std::thread t(&BasicRPCServer::LaunchServer, this, services);
    t.detach();
}

void BasicRPCServer::Shutdown() {
    isRunning_ = false;
}

bool BasicRPCServer::IsRunning() {
    return isRunning_.load();
}

void BasicRPCServer::LaunchServer(const std::vector<grpc::Service*>& services) {
    if (services.empty()) {
        spdlog::warn("Please specify at least one RPC service!");
        return;
    }

    grpc::ServerBuilder builder;
    builder.AddListeningPort(this->server_address_, grpc::InsecureServerCredentials());

    for (auto& service : services) {
        builder.RegisterService(service);
    }

    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
    if (!server) {
        spdlog::error("Failed to create the rpc server, please check the address {}", server_address_);
        return;
    } else {
        isRunning_ = true;
        spdlog::info("RPC Server is running on {}", this->server_address_);
    }

    while (IsRunning()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    spdlog::info("RPC Server is shutting down");
    server->Shutdown();
}
