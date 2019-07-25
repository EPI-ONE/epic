#ifndef EPIC_VERSION_MESSAGE_H
#define EPIC_VERSION_MESSAGE_H

#include "net_address.h"
#include "net_message.h"
#include "serialize.h"

class VersionMessage : public NetMessage {
public:
    int client_version     = 0;
    uint64_t local_service = 0;
    uint64_t nTime;
    NetAddress address_you;
    uint64_t current_height;

    explicit VersionMessage() : NetMessage(VERSION_MSG) {}

    VersionMessage(NetAddress address_you,
                   uint64_t current_height,
                   int client_version     = 0,
                   uint64_t local_service = 0,
                   uint64_t nTime         = time(nullptr))
        : NetMessage(VERSION_MSG), client_version(client_version), local_service(local_service), nTime(nTime),

          address_you(std::move(address_you)), current_height(current_height) {}

    explicit VersionMessage(VStream& stream) : NetMessage(VERSION_MSG) {
        Deserialize(stream);
    }

    ADD_SERIALIZE_METHODS
    ADD_NET_SERIALIZE_METHODS
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(client_version);
        READWRITE(local_service);
        READWRITE(nTime);
        READWRITE(address_you);
        READWRITE(current_height);
    }

    /**
     * get a fake version message for test
     * @return fake version message
     */
    static VersionMessage GetFakeVersionMessage() {
        VersionMessage versionMessage{};
        versionMessage.client_version = 0;
        versionMessage.current_height = 0;
        versionMessage.local_service  = 0;
        versionMessage.nTime          = time(nullptr);
        return versionMessage;
    }
};

#endif // EPIC_VERSION_MESSAGE_H
