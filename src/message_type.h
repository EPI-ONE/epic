#ifndef EPIC_MESSAGE_TYPE_H
#define EPIC_MESSAGE_TYPE_H
#include "version_message.h"
#include "version_ack.h"
#include "ping.h"
#include "pong.h"
#include "block.h"
#include "bundle.h"
#include "address_message.h"
#include "getaddr_message.h"

enum MessageType {
    PING = 0,
    PONG,
    VERSION_MSG,
    VERSION_ACK,
    GET_ADDR,
    ADDR,
    BLOCK,
    BUNDLE,
};

#endif //EPIC_MESSAGE_TYPE_H
