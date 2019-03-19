#ifndef EPIC_VERSION_MESSAGE_H
#define EPIC_VERSION_MESSAGE_H

#include <net_address.h>

class VersionMessage {
    private:
        // Serialize (may be changed since we haven't decided the P2P protocol)
        int client_version_;
        long local_service_;
        long nTime_;
        NetAddress address_you_;
        NetAddress address_me_;
        long peer_id_;
        long current_height_;

    public:
        VersionMessage(int client_version, long local_service, long nTime, NetAddress address_you,
                       NetAddress address_me, long peer_id, long current_height) :
                client_version_(client_version), local_service_(local_service), nTime_(nTime),
                address_you_(address_you), address_me_(address_me), peer_id_(peer_id),
                current_height_(current_height) {}

};

#endif //EPIC_VERSION_MESSAGE_H
