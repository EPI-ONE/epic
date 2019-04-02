#ifndef EPIC_ADDRESS_MESSAGE_H
#define EPIC_ADDRESS_MESSAGE_H

#include <vector>
#include <net_address.h>
#include "serialize.h"

class AddressMessage {
    public:
        static const long MAX_ADDRESSES_SIZE = 1024;

        std::vector<NetAddress> address_list_;

        AddressMessage() = default;

        explicit AddressMessage(std::vector<NetAddress> &&address_list) : address_list_(address_list) {}

        ~AddressMessage() {
            address_list_.clear();
        }

        ADD_SERIALIZE_METHODS;
        template<typename Stream, typename Operation>
        inline void SerializationOp(Stream &s, Operation ser_action) {
            READWRITE(address_list_);
        }
};

#endif //EPIC_ADDRESS_MESSAGE_H
