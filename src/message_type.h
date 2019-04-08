#ifndef EPIC_MESSAGE_TYPE_H
#define EPIC_MESSAGE_TYPE_H

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
