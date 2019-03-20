#ifndef EPIC_PEER_MANAGER_H
#define EPIC_PEER_MANAGER_H

#include <map>
#include <functional>
#include <event2/util.h>
#include <peer.h>
#include <net_address.h>

class PeerManager {

    private:
        std::map<long, Peer*> peer_map_;
        Peer* createPeer(evutil_socket_t socket_id, NetAddress address);
    public:
        PeerManager();

        ~PeerManager();

        void onConnectionCreated(evutil_socket_t socket_id, std::string address);

        void onConnectionClosed(evutil_socket_t socket_id);
};


#endif //EPIC_PEER_MANAGER_H
