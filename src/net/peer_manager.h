#ifndef EPIC_PEER_MANAGER_H
#define EPIC_PEER_MANAGER_H

#if defined(__SUPPORT_TS_ANNOTATION__) || defined(__clang__)
#define THREAD_ANNOTATION_ATTRIBUTE__(x)   __attribute__((x))
#else
#define THREAD_ANNOTATION_ATTRIBUTE__(x)   // no-op
#endif

#define GUARDED_BY(x) THREAD_ANNOTATION_ATTRIBUTE__(guarded_by(x))
//#include "a.h"
#include <unordered_map>
#include <functional>
#include <optional>
#include <mutex>
#include <shared_mutex>

#include "spdlog.h"
#include "peer.h"
#include "net_address.h"
#include "connection_manager.h"
#include "address_manager.h"

class PeerManager {

    public:
        PeerManager();

        ~PeerManager();

        void Start();

        void Stop();

        /**
         * bind and listen to a local address
         * @param bindAddress
         * @return true if success
         */
        bool Bind(NetAddress &bindAddress);

        /**
         * bind and listen to a local address string
         * @param bindAddress
         * @return true if success
         */
        bool Bind(const std::string &bindAddress);

        /**
         * connect to a specified address
         * @param connectTo
         * @return true if success
         */
        bool ConnectTo(NetAddress &connectTo);

        /**
         * connect to a specified address string
         * @param connectTo
         * @return true if success
         */
        bool ConnectTo(const std::string &connectTo);

        /**
         * the callback function for connection manager to call when connect() or accept() event happens
         * @param connection_handle
         * @param address
         * @param inbound
         */
        void OnConnectionCreated(void *connection_handle, std::string address, bool inbound);

        /**
         * the callback function for connection manager to call when disconnect event happens
         * @param connection_handle
         */
        void OnConnectionClosed(void *connection_handle);

        /**
         * get size of all peers(including not fully connected ones)
         * @return
         */
        size_t GetConnectedPeerSize();

    private:
        /**
         * create a peer after a new connection is setup
         * @param connection_handle
         * @param address
         * @param inbound
         * @return
         */
        Peer *CreatePeer(void *connection_handle, NetAddress address, bool inbound);

        /**
         * release resources of a peer and then delete it
         * @param peer
         */
        void DeletePeer(Peer *peer);


        /**
         * default network parameter based on the protocol
         */

        // possibility of relaying a block to a peer
        constexpr static float kAlpha = 0.5;

        /**
         * current local network status
         */

        // TODO

        /**
         * internal data structures
         */
        // peers' lock
        std::mutex peerLock_;

        // a map to save all peers
        std::unordered_map<void *, Peer *> peerMap_ GUARDED_BY(peerLock_);

        // connection manager
        ConnectionManager *connectionManager_;

        // address manager
        AddressManager *addressManager_;

};

extern std::unique_ptr<PeerManager> peerManager;
#endif //EPIC_PEER_MANAGER_H
