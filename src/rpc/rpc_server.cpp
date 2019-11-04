// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "rpc_server.h"
#include "milestone.h"
#include "service/basic_block_explorer.h"
#include "service/command_line.h"

RPCServer::RPCServer(const NetAddress& address, const std::vector<RPCServiceType>& services)
    : server_(address.ToString()) {
    for (auto& type : services) {
        switch (type) {
            case RPCServiceType::BLOCK_EXPLORER_SERVER: {
                service_impls_.emplace_back(std::make_unique<BasicBlockExplorerRPCServiceImpl>());
                break;
            }
            case RPCServiceType::COMMAND_LINE_SERVER: {
                service_impls_.emplace_back(std::make_unique<CommanderRPCServiceImpl>());
                break;
            }
            default:
                break;
        }
    }
}

bool RPCServer::IsRunning() {
    return server_.IsRunning();
}

void RPCServer::Start() {
    std::vector<grpc::Service*> services;
    for (auto& s : service_impls_) {
        services.push_back(s.get());
    }
    server_.Start(services);
}

void RPCServer::Shutdown() {
    server_.Shutdown();
}

RPCServer::~RPCServer() = default;
