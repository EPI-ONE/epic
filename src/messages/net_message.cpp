// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "net_message.h"
#include "address_message.h"
#include "ping.h"
#include "pong.h"
#include "sync_messages.h"
#include "version_message.h"

unique_message_t NetMessage::MessageFactory(uint8_t type, uint8_t countDown, VStream& s) {
    unique_message_t msg;
    try {
        switch (type) {
            case PING:
                msg = std::make_unique<Ping>(s);
                break;
            case PONG:
                msg = std::make_unique<Pong>(s);
                break;
            case VERSION_MSG:
                msg = std::make_unique<VersionMessage>(s);
                break;
            case VERSION_ACK:
                msg = std::make_unique<NetMessage>(VERSION_ACK);
                break;
            case GET_ADDR:
                msg = std::make_unique<NetMessage>(GET_ADDR);
                break;
            case ADDR:
                msg = std::make_unique<AddressMessage>(s);
                break;
            case TX:
                msg = std::make_unique<Transaction>(s);
                break;
            case BLOCK:
                msg = std::make_unique<Block>(s);
                break;
            case BUNDLE:
                msg = std::make_unique<Bundle>(s);
                break;
            case GET_INV:
                msg = std::make_unique<GetInv>(s);
                break;
            case INV:
                msg = std::make_unique<Inv>(s);
                break;
            case GET_DATA:
                msg = std::make_unique<GetData>(s);
                break;
            case NOT_FOUND:
                msg = std::make_unique<NotFound>(s);
                break;
            default:
                msg = std::make_unique<NetMessage>(NONE);
                break;
        }
    } catch (std::exception& e) {
        spdlog::warn("message {} deserialize error {}", type, e.what());
    }

    msg->SetCount(countDown);

    return msg;
}
