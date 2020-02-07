// Copyright (c) 2020 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef EPIC_SUBSCRIPTION_H
#define EPIC_SUBSCRIPTION_H

#include "rpc_tools.h"

#include <grpc++/grpc++.h>
#include <mutex>
#include <rpc.grpc.pb.h>
#include <rpc.pb.h>
#include <string>
#include <unordered_map>

enum SubType { BLOCK = 1, TX = 2 };

class Subscriber {
public:
    Subscriber(std::unique_ptr<rpc::Subscription::Stub>&& stub, uint8_t service_);
    bool PushBlock(rpc::Vertex& vertex) const;
    bool PushTx(rpc::Transaction& tx) const;
    std::uint8_t service;

private:
    std::unique_ptr<rpc::Subscription::Stub> push_stub_;
};


class Publisher {
public:
    bool AddNewSubscriber(std::string address, uint8_t service);
    void DeleteSubscriber(std::string address);
    uint32_t GetSubscriberCount();
    void PushMsg(void* msg, SubType type);

private:
    std::unordered_map<std::string, Subscriber> subscribers_;
    std::recursive_mutex lock_;
};

extern std::unique_ptr<Publisher> PUBLISHER;

#endif
