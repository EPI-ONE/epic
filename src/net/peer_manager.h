#ifndef EPIC_PEER_MANAGER_H
#define EPIC_PEER_MANAGER_H

#include <functional>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <unordered_map>

#include "address_manager.h"
#include "connection_manager.h"
#include "net_address.h"
#include "peer.h"
#include "spdlog.h"

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
    bool Bind(NetAddress& bindAddress);

    /**
     * bind and listen to a local address string
     * @param bindAddress
     * @return true if success
     */
    bool Bind(const std::string& bindAddress);

    /**
     * connect to a specified address
     * @param connectTo
     * @return true if success
     */
    bool ConnectTo(NetAddress& connectTo);

    /**
     * connect to a specified address string
     * @param connectTo
     * @return true if success
     */
    bool ConnectTo(const std::string& connectTo);

    /**
     * the callback function for connection manager to call when connect() or accept() event happens
     * @param connection_handle
     * @param address
     * @param inbound
     */
    void OnConnectionCreated(void* connection_handle, std::string address, bool inbound);

    /**
     * the callback function for connection manager to call when disconnect event happens
     * @param connection_handle
     */
    void OnConnectionClosed(void* connection_handle);

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
    Peer* CreatePeer(void* connection_handle, NetAddress address, bool inbound);

    /**
     * release resources of a peer and then delete it
     * @param peer
     */
    void DeletePeer(Peer* peer);


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
    std::unordered_map<void*, Peer*> peerMap_;

    // connection manager
    ConnectionManager* connectionManager_;

    // address manager
    AddressManager* addressManager_;
};

extern std::unique_ptr<PeerManager> peerManager;
#endif // EPIC_PEER_MANAGER_H
