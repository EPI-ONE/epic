#ifndef EPIC_PING_H
#define EPIC_PING_H
#include "serialize.h"
#include "net_message.h"

class Ping {
public:
    // Serialize
    uint64_t nonce = 0;

    Ping() = default;
    explicit Ping(uint64_t nonce) : nonce(nonce) {
    }
    ADD_SERIALIZE_METHODS;
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(nonce);
    }
};

#endif // EPIC_PING_H
