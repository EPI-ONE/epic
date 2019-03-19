#ifndef EPIC_PEER_MANAGER_H
#define EPIC_PEER_MANAGER_H

#include <map>
#include <functional>

#include <peer.h>
#include <net_address.h>

class PeerManager {

    private:
        std::map<long, Peer*> peer_map_;
        Peer* createPeer(long socket_id, NetAddress address);
    public:
        PeerManager();

        ~PeerManager();

        void onConnectionCreated(long socket_id, NetAddress address);

        void onConnectionClosed(long socket_id);
};


#endif //EPIC_PEER_MANAGER_H
