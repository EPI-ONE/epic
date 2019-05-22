#ifndef __SRC_MESSAGE_TYPE_H__
#define __SRC_MESSAGE_TYPE_H__

enum MessageType : uint8_t {
    PING = 0,
    PONG,
    VERSION_MSG,
    VERSION_ACK,
    GET_ADDR,
    ADDR,
    BLOCK,
    BUNDLE,
    GET_BLOCK,
    INV,
    GET_DATA,
};

#endif //__SRC_MESSAGE_TYPE_H__
