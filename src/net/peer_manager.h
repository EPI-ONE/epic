#ifndef EPIC_PEER_MANAGER_H
#define EPIC_PEER_MANAGER_H

#include <ctime>
#include <functional>
#include <memory>
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

    virtual ~PeerManager();

    void Start();

    void Stop();

    bool Init(std::unique_ptr<Config>& config);

    /**
     * listen
     * @param port
     * @return true if success
     */
    bool Listen(uint16_t port);

    /**
     * bind a local address
     * @param bindAddress
     * @return true if success
     */
    bool Bind(IPAddress& bindAddress);

    /**
     * bind a local address string
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
    void OnConnectionClosed(const void* connection_handle);

    /*
     * get size of all peers(including not fully connected ones)
     * @return
     */
    size_t GetConnectedPeerSize();


    /*
     * get size of all peers(including not fully connected ones)
     * @return
     */
    size_t GetFullyConnectedPeerSize();

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

    /**
     * regularly broadcast local address to peers
     */
    void SendLocalAddresses();

    /**
     * get the pointer of peer via the connection handle
     * @param connection_handle
     * @return
     */
    virtual PeerPtr GetPeer(const void* connection_handle);

    /**
     * relay block to neighbors
     * @param block
     */
    void RelayBlock(const ConstBlockPtr& block, const PeerPtr& msg_from);

protected:
    /*
     * create a peer after a new connection is setup
     * @param connection_handle
     * @param address
     * @param inbound
     * @return
     */
    PeerPtr CreatePeer(void* connection_handle, NetAddress& address, bool inbound);

    void RemovePeer(const void* connection_handle);

    /*
     * release resources of a peer and then delete it
     * @param peer
     */
    void DeletePeer(const PeerPtr& peer);

    void DisconnectPeer(PeerPtr& peer);

    /*
     * check if we have connected to the ip address
     * @return
     */
    bool HasConnectedTo(const IPAddress& address);

    /**
     * add a peer into peer map
     * @param handle
     * @param peer
     */
    void AddPeer(const void* handle, const PeerPtr& peer);

    /**
     * add a connected address
     * @param address
     */
    void AddAddr(const NetAddress& address);

    /**
     * remove a connected address
     * @param address
     */
    void RemoveAddr(const NetAddress& address);

    /**
     * a while loop function to receive and process messages
     */
    void HandleMessage();

    /**
     * a while loop function to setup outbound connection
     */
    void OpenConnection();

    /**
     * a while loop to check the behaviors of peers and disconnect bad peer
     */
    void ScheduleTask();

    void CheckTimeout();

    void BroadcastMessage();


    /*
     * default network parameter based on the protocol
     */

    // possibility of relaying a block to a peer
    constexpr static float kAlpha = 0.5;

    // max outbound size
    const size_t kMax_outbound = 8;

    // The default timeout between when a connection attempt begins and version
    // message exchange completes
    const static int kConnectionSetupTimeout = 3 * 60;

    // broadcast local address per 24h
    const static long kBroadLocalAddressInterval = 24 * 60 * 60;

    // timeout between sending ping and receiving pong
    const static long kPingWaitTimeout = 3 * 60;

    // max number of ping failures
    const static size_t kMaxPingFailures = 3;

    /*
     * current local network status
     */

    // last time of sending local address
    long lastSendLocalAddressTime{0};

    /*
     * internal data structures
     */

    // peers' lock
    std::recursive_mutex peerLock_;

    // a map to save all peers
    std::unordered_map<const void*, PeerPtr> peerMap_;

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
    std::atomic_bool interrupt_ = false;

    // handle message
    std::thread handleMessageTask_;

    // continuously choose addresses and connect to them
    std::thread openConnectionTask_;

    // do some periodical tasks
    std::thread scheduleTask_;

    std::string connect_;
};

extern std::unique_ptr<PeerManager> peerManager;
#endif // EPIC_PEER_MANAGER_H
