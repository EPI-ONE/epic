#ifndef __SRC_ADDRESS_MESSAGE_H__
#define __SRC_ADDRESS_MESSAGE_H__

#include <net_address.h>
#include <utils/stream.h>
#include <vector>

#include "serialize.h"

class AddressMessage {
public:
    static const long kMaxAddressSize = 1024;

    std::vector<NetAddress> addressList;

    AddressMessage() = default;

    explicit AddressMessage(VStream& stream) {
        Deserialize(stream);
    }

    explicit AddressMessage(std::vector<NetAddress>& address_list) : addressList(std::move(address_list)) {}

    void AddAddress(NetAddress& addr) {
        addressList.push_back(addr);
    }

    void AddAddress(NetAddress&& addr) {
        addressList.push_back(addr);
    }

    ADD_SERIALIZE_METHODS;
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(addressList);
    }
};

#endif // __SRC_ADDRESS_MESSAGE_H__
