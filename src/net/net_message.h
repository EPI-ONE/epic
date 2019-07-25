#ifndef EPIC_NET_MESSAGE_H
#define EPIC_NET_MESSAGE_H

#include "event2/util.h"
#include "stream.h"

#define ADD_NET_SERIALIZE_METHODS             \
    virtual void NetSerialize(VStream& s) {   \
        Serialize(s);                         \
    }                                         \
    virtual void NetDeserialize(VStream& s) { \
        Deserialize(s);                       \
    }

class NetMessage;

typedef std::unique_ptr<NetMessage> unique_message_t;

class NetMessage {
public:
    enum Type {
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
        NONE,
    };

    explicit NetMessage(NetMessage::Type type = NONE) : type_(type) {}

    virtual ~NetMessage() {}

    static unique_message_t MessageFactory(uint32_t type, VStream& s);

    NetMessage::Type GetType() const {
        return type_;
    }

    virtual void NetSerialize(VStream& s) {}
    virtual void NetDeserialize(VStream& s) {}

protected:
    NetMessage::Type type_;
};

#endif // EPIC_NET_MESSAGE_H
