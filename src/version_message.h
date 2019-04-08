#ifndef EPIC_VERSION_MESSAGE_H
#define EPIC_VERSION_MESSAGE_H

#include "net_address.h"
#include "serialize.h"

class VersionMessage {
    public:
        int client_version;
        uint64_t local_service;
        uint64_t nTime;
        NetAddress address_you;
        NetAddress address_me;
        uint64_t current_height;

        VersionMessage() = default;
        VersionMessage(int client_version, uint64_t local_service, uint64_t nTime, NetAddress &address_you,
                       NetAddress &address_me, uint64_t current_height) :
            client_version(client_version), local_service(local_service), nTime(nTime),
            address_you(std::move(address_you)), address_me(std::move(address_me)),
            current_height(current_height) {}

        ADD_SERIALIZE_METHODS;
        template<typename Stream, typename Operation>
        inline void SerializationOp(Stream &s, Operation ser_action) {
            READWRITE(client_version);
            READWRITE(local_service);
            READWRITE(nTime);
            READWRITE(address_you);
            READWRITE(address_me);
            READWRITE(current_height);
        }

        /**
         * get a fake version message for test
         * @return fake version message
         */
        static VersionMessage GetFakeVersionMessage() {
            VersionMessage versionMessage;
            versionMessage.client_version = 0;
            versionMessage.current_height = 0;
            versionMessage.local_service = 0;
            versionMessage.nTime = time(nullptr);
            versionMessage.address_me = *NetAddress::StringToNetAddress("127.0.0.1:7877");
            return versionMessage;
        }
};

#endif //EPIC_VERSION_MESSAGE_H
