// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "address_message.h"
#include "ping.h"
#include "pong.h"
#include "sync_messages.h"
#include "version_message.h"

unique_message_t NetMessage::MessageFactory(uint32_t type, VStream& s) {
    try {
        switch (type) {
            case PING:
                return std::make_unique<Ping>(s);
            case PONG:
                return std::make_unique<Pong>(s);
            case VERSION_MSG:
                return std::make_unique<VersionMessage>(s);
            case VERSION_ACK:
                return std::make_unique<NetMessage>(VERSION_ACK);
            case GET_ADDR:
                return std::make_unique<NetMessage>(GET_ADDR);
            case ADDR:
                return std::make_unique<AddressMessage>(s);
            case TX:
                return std::make_unique<Transaction>(s);
            case BLOCK:
                return std::make_unique<Block>(s);
            case BUNDLE:
                return std::make_unique<Bundle>(s);
            case GET_INV:
                return std::make_unique<GetInv>(s);
            case INV:
                return std::make_unique<Inv>(s);
            case GET_DATA:
                return std::make_unique<GetData>(s);
            case NOT_FOUND:
                return std::make_unique<NotFound>(s);
            default:
                break;
        }
    } catch (std::exception& e) {
        spdlog::warn("message {} deserialize error {}", type, e.what());
    }

    return std::make_unique<NetMessage>(NONE);
}
