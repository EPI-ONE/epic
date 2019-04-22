#ifndef EPIC_ADDRESS_MESSAGE_H
#define EPIC_ADDRESS_MESSAGE_H

#include "serialize.h"
#include <net_address.h>
#include <vector>

class AddressMessage {
public:
    static const long kMaxAddressSize = 1024;

    std::vector<NetAddress> addressList;

    AddressMessage() = default;

    explicit AddressMessage(std::vector<NetAddress>& address_list) : addressList(std::move(address_list)) {
    }

    ~AddressMessage() {
        addressList.clear();
    }

    ADD_SERIALIZE_METHODS;
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(addressList);
    }
};

#endif // EPIC_ADDRESS_MESSAGE_H
