#ifndef EPIC_PEER_MANAGER_H
#define EPIC_PEER_MANAGER_H

#include <ctime>
#include <functional>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <unordered_map>
#include <unordered_set>

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

    /*
     * bind and listen to a local address
     * @param bindAddress
     * @return true if success
     */
    bool Bind(NetAddress& bindAddress);

    /*
     * bind and listen to a local address string
     * @param bindAddress
     * @return true if success
     */
    bool Bind(const std::string& bindAddress);

    /*
     * connect to a specified address
     * @param connectTo
     * @return true if success
     */
    bool ConnectTo(NetAddress& connectTo);

    /*
     * connect to a specified address string
     * @param connectTo
     * @return true if success
     */
    bool ConnectTo(const std::string& connectTo);

    /*
     * the callback function for connection manager to call when connect() or
     * accept() event happens
     * @param connection_handle
     * @param address
     * @param inbound
     */
    void OnConnectionCreated(void* connection_handle, const std::string& address, bool inbound);

    /*
     * the callback function for connection manager to call when disconnect
     * event happens
     * @param connection_handle
     */
    void OnConnectionClosed(void* connection_handle);

    /*
     * get size of all peers(including not fully connected ones)
     * @return
     */
    size_t GetConnectedPeerSize();

    /*
     * send a message to specified peer
     * @param message
     */
    void SendMessage(NetMessage& message);

    /*
     * send a message to specified peer
     * @param message
     */
    void SendMessage(NetMessage&& message);

private:
    /*
     * create a peer after a new connection is setup
     * @param connection_handle
     * @param address
     * @param inbound
     * @return
     */
    Peer* CreatePeer(void* connection_handle, NetAddress& address, bool inbound);

    /*
     * release resources of a peer and then delete it
     * @param peer
     */
    void DeletePeer(Peer* peer);

    /*
     * check if we have connected to the ip address
     * @return
     */
    bool HasConnectedTo(const IPAddress& address);

    /*
     * get the pointer of peer via the connection handle
     * @param connection_handle
     * @return
     */
    Peer* GetPeer(const void* connection_handle);

    /*
     * add a peer into peer map
     * @param handle
     * @param peer
     */
    void AddPeer(const void* handle, Peer* peer);

    /*
     * add a connected address
     * @param address
     */
    void AddAddr(const NetAddress& address);

    /*
     * remove a connected address
     * @param address
     */
    void RemoveAddr(const NetAddress& address);

    /*
     * a while loop function to receive and process messages
     */
    void HandleMessage();

    /*
     * default network parameter based on the protocol
     */

    // possibility of relaying a block to a peer
    constexpr static float kAlpha = 0.5;

    /*
     * TODO: current local network status
     */


    /*
     * internal data structures
     */

    // peers' lock
    std::mutex peerLock_;

    // a map to save all peers
    std::unordered_map<const void*, Peer*> peerMap_;

    // lock of connected address set
    std::mutex addressLock_;

    // a set to save connected addresses
    std::unordered_set<IPAddress> connectedAddress_;

    // address manager
    AddressManager* addressManager_;

    // connection manager
    ConnectionManager* connectionManager_;

    /*
     * threads
     */

    // handle message
    std::atomic_bool interrupt_ = false;
    std::thread handleMessageTask_;
};

extern std::unique_ptr<PeerManager> peerManager;
#endif // EPIC_PEER_MANAGER_H
