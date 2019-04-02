#ifndef EPIC_GETADDR_MESSAGE_H
#define EPIC_GETADDR_MESSAGE_H
#include "serialize.h"

class GetAddrMessage{

    public:
        GetAddrMessage()= default;
        ADD_SERIALIZE_METHODS;
        template<typename Stream, typename Operation>
        inline void SerializationOp(Stream &s, Operation ser_action) {
        }
};
#endif //EPIC_GETADDR_MESSAGE_H
