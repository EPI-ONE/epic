// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef EPIC_VERSION_MESSAGE_H
#define EPIC_VERSION_MESSAGE_H

#include "net_address.h"
#include "net_message.h"
#include "serialize.h"

class VersionMessage : public NetMessage {
public:
    int client_version     = 0;
    uint64_t local_service = 0;
    uint64_t nTime;
    NetAddress address_you;
    NetAddress address_me;
    uint64_t current_height;
    uint64_t id;

    explicit VersionMessage() : NetMessage(VERSION_MSG) {}

    VersionMessage(NetAddress address_you_,
                   NetAddress address_me_,
                   uint64_t current_height,
                   uint64_t id_,
                   int client_version     = 0,
                   uint64_t local_service = 0,
                   uint64_t nTime         = time(nullptr))
        : NetMessage(VERSION_MSG), client_version(client_version), local_service(local_service), nTime(nTime),
          address_you(std::move(address_you_)), address_me(std::move(address_me_)), current_height(current_height),
          id(id_) {}

    explicit VersionMessage(VStream& stream) : NetMessage(VERSION_MSG) {
        Deserialize(stream);
    }

    ADD_SERIALIZE_METHODS
    ADD_NET_SERIALIZE_METHODS
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(client_version);
        READWRITE(local_service);
        READWRITE(nTime);
        READWRITE(address_you);
        READWRITE(address_me);
        READWRITE(current_height);
        READWRITE(id);
    }
};

#endif // EPIC_VERSION_MESSAGE_H
