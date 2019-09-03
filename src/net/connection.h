// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef EPIC_CONNECTION_H
#define EPIC_CONNECTION_H

#include "net_message.h"

#include <atomic>
#include <event2/bufferevent.h>
#include <string>

class ConnectionManager;
class Connection;

typedef struct bufferevent bufferevent_t;
typedef std::shared_ptr<Connection> shared_connection_t;

class Connection {
public:
    Connection(bufferevent_t* bev, bool inbound, std::string& remote, ConnectionManager* cmptr)
        : valid_(true), bev_(bev), inbound_(inbound), length_(0), cmptr_(cmptr), remote_(remote) {
        connection_ = shared_connection_t(this);
    }

    ~Connection() {
        bufferevent_free(bev_);
    }

    bool IsValid() const {
        return valid_;
    }

    ConnectionManager* GetCmptr() const {
        return cmptr_;
    }

    const std::string& GetRemote() const {
        return remote_;
    }

    bool IsInbound() const {
        return inbound_;
    }

    bufferevent_t* GetBev() const {
        return bev_;
    }

    size_t& GetLength() {
        return length_;
    }

    shared_connection_t& GetHandlePtr() {
        return connection_;
    }

    void Release();

    void Disconnect();

    void SendMessage(unique_message_t&& message);

private:
    std::atomic_bool valid_;
    bufferevent_t* bev_;
    bool inbound_;
    size_t length_;
    ConnectionManager* cmptr_;
    std::string remote_;
    shared_connection_t connection_;
};


#endif // EPIC_CONNECTION_H
