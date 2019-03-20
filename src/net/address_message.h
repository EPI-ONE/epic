#ifndef EPIC_ADDRESS_MESSAGE_H
#define EPIC_ADDRESS_MESSAGE_H

#include <list>
#include <net_address.h>

class AddressMessage {
    private:
        // Serialize
        std::list<NetAddress> address_list_;

    public:
        static const long MAX_ADDRESSES_SIZE = 1024;

        AddressMessage(std::list<NetAddress> &&address_list) : address_list_(address_list) {}

        ~AddressMessage() {
            address_list_->clear();
        }
};

#endif //EPIC_ADDRESS_MESSAGE_H
