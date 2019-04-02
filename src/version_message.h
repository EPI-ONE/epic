#ifndef EPIC_VERSION_MESSAGE_H
#define EPIC_VERSION_MESSAGE_H

#include "net_address.h"
#include "serialize.h"

class VersionMessage {
    public:
        int client_version_;
        uint64_t local_service_;
        uint64_t nTime_;
        NetAddress address_you_;
        NetAddress address_me_;
        uint64_t current_height_;

        VersionMessage() = default;
        VersionMessage(int client_version, uint64_t local_service, uint64_t nTime, NetAddress &address_you,
                       NetAddress &address_me, uint64_t current_height) :
            client_version_(client_version), local_service_(local_service), nTime_(nTime),
            address_you_(std::move(address_you)), address_me_(std::move(address_me)),
            current_height_(current_height) {}

        ADD_SERIALIZE_METHODS;
        template<typename Stream, typename Operation>
        inline void SerializationOp(Stream &s, Operation ser_action) {
            READWRITE(client_version_);
            READWRITE(local_service_);
            READWRITE(nTime_);
            READWRITE(address_you_);
            READWRITE(address_me_);
            READWRITE(current_height_);
        }
};

#endif //EPIC_VERSION_MESSAGE_H
