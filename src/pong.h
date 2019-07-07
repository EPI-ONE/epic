#ifndef __SRC_PONG_H__
#define __SRC_PONG_H__

#include "serialize.h"

class Pong {
public:
    uint64_t nonce = 0;

    Pong() = default;

    explicit Pong(uint64_t nonce) : nonce(nonce) {}

    explicit Pong(VStream& stream) {
        Deserialize(stream);
    }

    ADD_SERIALIZE_METHODS;
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(nonce);
    }
};

#endif // EPIC_PONG_H
