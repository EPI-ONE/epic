#ifndef __SRC_ADDRESS_MESSAGE_H__
#define __SRC_ADDRESS_MESSAGE_H__


#include <vector>

#include "net_address.h"
#include "net_message.h"
#include "serialize.h"
#include "stream.h"

class AddressMessage : public NetMessage {
public:
    static const long kMaxAddressSize = 1024;

    std::vector<NetAddress> addressList;

    AddressMessage() : NetMessage(ADDR) {}

    explicit AddressMessage(VStream& stream) : NetMessage(ADDR) {
        Deserialize(stream);
    }

    explicit AddressMessage(std::vector<NetAddress>&& address_list) : NetMessage(ADDR), addressList(address_list) {}

    void AddAddress(NetAddress& addr) {
        addressList.push_back(addr);
    }

    void AddAddress(NetAddress&& addr) {
        addressList.push_back(addr);
    }

    ADD_SERIALIZE_METHODS
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(addressList);
    }
    ADD_NET_SERIALIZE_METHODS
};

class GetAddrMessage {
public:
    GetAddrMessage() = default;

    ADD_SERIALIZE_METHODS
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream&, Operation) {}
};

#endif // __SRC_ADDRESS_MESSAGE_H__
