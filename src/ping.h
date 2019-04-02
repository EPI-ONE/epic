#ifndef EPIC_PING_H
#define EPIC_PING_H
#include "serialize.h"

class Ping {
    public:
        // Serialize
        uint64_t nonce_ = 0;

        Ping() = default;
        explicit Ping(uint64_t nonce) : nonce_(nonce) {}
        ADD_SERIALIZE_METHODS;
        template<typename Stream, typename Operation>
        inline void SerializationOp(Stream &s, Operation ser_action) {
            READWRITE(nonce_);
        }
};

#endif //EPIC_PING_H
