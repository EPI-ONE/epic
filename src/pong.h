#ifndef EPIC_PONG_H
#define EPIC_PONG_H
#include "serialize.h"

class Pong {
public:
    uint64_t nonce = 0;

    Pong() = default;
    explicit Pong(uint64_t nonce) : nonce(nonce) {
    }
    ADD_SERIALIZE_METHODS;
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(nonce);
    }
};

#endif // EPIC_PONG_H
