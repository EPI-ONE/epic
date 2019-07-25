#ifndef __SRC_PING_H__
#define __SRC_PING_H__

#include "net_message.h"
#include "serialize.h"

class Ping : public NetMessage {
public:
    // Serialize
    uint64_t nonce = 0;

    explicit Ping() : NetMessage(PING) {}

    explicit Ping(uint64_t nonce) : NetMessage(PING), nonce(nonce) {}

    explicit Ping(VStream& stream) : NetMessage(PING) {
        Deserialize(stream);
    }

    ADD_SERIALIZE_METHODS
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(nonce);
    }
    ADD_NET_SERIALIZE_METHODS
};

#endif // EPIC_PING_H
