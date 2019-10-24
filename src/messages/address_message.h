// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef EPIC_ADDRESS_MESSAGE_H
#define EPIC_ADDRESS_MESSAGE_H

#include "net_address.h"
#include "net_message.h"
#include "serialize.h"
#include "stream.h"

#include <vector>

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
        addressList.emplace_back(addr);
    }

    ADD_SERIALIZE_METHODS
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(addressList);
    }
    ADD_NET_SERIALIZE_METHODS
};

#endif // EPIC_ADDRESS_MESSAGE_H
