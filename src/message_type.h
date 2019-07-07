#ifndef __SRC_MESSAGE_TYPE_H__
#define __SRC_MESSAGE_TYPE_H__

#include <cstdint>

enum MessageType : uint8_t {
    PING = 0,
    PONG,
    VERSION_MSG,
    VERSION_ACK,
    GET_ADDR,
    ADDR,
    BLOCK,
    BUNDLE,
    GET_INV,
    INV,
    GET_DATA,
    NOT_FOUND,
};

#endif //__SRC_MESSAGE_TYPE_H__
