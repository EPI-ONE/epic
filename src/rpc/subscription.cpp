// Copyright (c) 2020 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "subscription.h"
#include "rpc.grpc.pb.h"
#include "rpc.pb.h"
#include "rpc_tools.h"
#include "spdlog.h"
#include "transaction.h"
#include "vertex.h"
#include <memory>
#include <mutex>
#include <string>
#include <vector>

Subscriber::Subscriber(std::unique_ptr<rpc::Subscription::Stub>&& stub, uint8_t service_)
    : service(service_), push_stub_(std::move(stub)) {}

bool Subscriber::PushBlock(rpc::Vertex& vertex) const {
    rpc::EmptyMessage response;
    grpc::ClientContext context;
    return push_stub_->PushBlock(&context, vertex, &response).ok();
}


bool Subscriber::PushTx(rpc::Transaction& tx) const {
    rpc::EmptyMessage response;
    grpc::ClientContext context;
    return push_stub_->PushTx(&context, tx, &response).ok();
}

bool Publisher::AddNewSubscriber(std::string address, uint8_t service) {
    auto stub =
        std::make_unique<rpc::Subscription::Stub>(grpc::CreateChannel(address, grpc::InsecureChannelCredentials()));
    if (stub) {
        std::unique_lock<std::recursive_mutex> lock(lock_);
        subscribers_.insert_or_assign(address, Subscriber{std::move(stub), service});
        spdlog::info("Add new subscriber {}", address);
        return true;
    }
    spdlog::warn("Failed to add subscriber {}, please check the connectability");
    return false;
}

void Publisher::DeleteSubscriber(std::string address) {
    std::unique_lock<std::recursive_mutex> lock(lock_);
    subscribers_.erase(address);
    spdlog::info("Delete subscriber {}", address);
}

uint32_t Publisher::GetSubscriberCount() {
    std::unique_lock<std::recursive_mutex> lock(lock_);
    return subscribers_.size();
}

void Publisher::PushMsg(void* msg, SubType type) {
    std::vector<std::string> toBeDel;
    std::unique_lock<std::recursive_mutex> lock(lock_);
    switch (type) {
        case SubType::BLOCK: {
            rpc::Vertex* vtx = ToRPCVertex(*(Vertex*) msg);
            for (const auto& it : subscribers_) {
                if (it.second.service & SubType::BLOCK) {
                    if(!it.second.PushBlock(*vtx)) {
                        toBeDel.emplace_back(it.first); 
                    }
                }
            }
            delete vtx;
            break;
        }
        case SubType::TX: {
            rpc::Transaction* tx = ToRPCTx(*(Transaction*) msg);
            for (auto& it : subscribers_) {
                if (it.second.service & SubType::TX) {
                    if(!it.second.PushTx(*tx)) {
                        toBeDel.emplace_back(it.first);
                    }
                }
            }
            delete tx;
            break;
        }
        default: {
            break;
        }
    }
    for(auto& it : toBeDel){
        DeleteSubscriber(it);
    }
}
