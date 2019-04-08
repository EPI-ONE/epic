#ifndef EPIC_ADDRESS_MANAGER_H
#define EPIC_ADDRESS_MANAGER_H
#include "net_address.h"

class AddressManager {
    public:

        /**
         * judge if a net address is a seed
         * @param address
         * @return bool
         */
        bool isSeedAddress(const NetAddress &address);
};

#endif //EPIC_ADDRESS_MANAGER_H
