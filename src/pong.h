#ifndef EPIC_PONG_H
#define EPIC_PONG_H
#include "serialize.h"

class Pong {
    public:
        uint64_t nonce_ = 0;

        Pong() = default;
        explicit Pong(uint64_t nonce) : nonce_(nonce) {}
        ADD_SERIALIZE_METHODS;
        template<typename Stream, typename Operation>
        inline void SerializationOp(Stream &s, Operation ser_action) {
            READWRITE(nonce_);
        }
};

#endif //EPIC_PONG_H
