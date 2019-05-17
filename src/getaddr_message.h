#ifndef __SRC_GETADDR_MESSAGE_H__
#define __SRC_GETADDR_MESSAGE_H__

#include "serialize.h"

class GetAddrMessage {
public:
    GetAddrMessage() = default;

    ADD_SERIALIZE_METHODS;
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream&, Operation) {}
};

#endif // __SRC_GETADDR_MESSAGE_H__
