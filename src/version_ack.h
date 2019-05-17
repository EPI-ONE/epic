#ifndef EPIC_VERSION_ACK_H
#define EPIC_VERSION_ACK_H

#include "serialize.h"

class VersionACK {
public:
    VersionACK() = default;

    ADD_SERIALIZE_METHODS;
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream&, Operation) {}
};

#endif // EPIC_VERSION_ACK_H
