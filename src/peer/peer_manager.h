// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef EPIC_PEER_MANAGER_H
#define EPIC_PEER_MANAGER_H

#include "peer.h"
#include "scheduler.h"

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
     */
    void OnConnectionCreated(shared_connection_t& connection);

    /*
     * the callback function for connection manager to call when disconnect
     * event happens
     * @param connection_handle
     */
    void OnConnectionClosed(shared_connection_t& connection);

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

    /**
     * get the pointer of peer via the connection handle
     * @param connection_handle
     * @return
     */
    PeerPtr GetPeer(shared_connection_t& connection);

    PeerPtr GetPeer(const std::string& address);

    std::vector<PeerPtr> GetAllPeer();

    /**
     * relay block to neighbors
     * @param ConstBlockPtr, PeerPtr
     */
    void RelayBlock(const ConstBlockPtr& block, const PeerPtr& msg_from);

    /**
     * relay transaction to neighbors
     * @param ConstTxPtr, PeerPtr
     */
    void RelayTransaction(const ConstTxPtr& tx, const PeerPtr& msg_from);

    /**
     * relay message to neighbors
     * @param message
     * @param msg_from
     */
    void RelayAddressMsg(AddressMessage& message, const PeerPtr& msg_from);

    /**
     * get my own peer id
     * @return myID_
     */
    uint64_t GetMyPeerID() const;

    void DisconnectAllPeer();

    void ClearPeers();

    bool DisconnectPeer(const std::string& address);

    bool InitialSyncCompleted() const;

    std::vector<std::string> GetConnectedPeers();

    std::vector<PeerPtr> RandomlySelect(size_t, const PeerPtr& excluded = nullptr);

private:
    /*
     * create a peer after a new connection is setup
     */
    PeerPtr CreatePeer(shared_connection_t& connection, NetAddress& address);

    void RemovePeer(shared_connection_t connection);

    /*
     * check if we have connected to the ip address
     * @return
     */
    bool HasConnectedTo(const NetAddress& address);

    /**
     * add a peer into peer map
     * @param handle
     * @param peer
     */
    void AddPeer(shared_connection_t& connection, const PeerPtr& peer);

    /**
     * a while loop function to receive and process messages
     */
    void HandleMessage();

    /**
     * process block, add to dag and relay
     * @param block
     */
    void ProcessBlock(const ConstBlockPtr& block, PeerPtr& peer);

    /**
     * process transaction, add to memory pool and relay
     * @param transaction
     */
    void ProcessTransaction(const ConstTxPtr& tx, PeerPtr& peer);

    /**
     * process address message, check, relay and save addresses
     * @param addressMessage
     */
    void ProcessAddressMessage(AddressMessage& addressMessage, PeerPtr& peer);

    bool CheckPeerID(uint64_t id);

    /**
     * a while loop function to setup outbound connection
     */
    void OpenConnection();

    /**
     * a while loop to check the behaviors of peers and disconnect bad peer
     */
    void ScheduleTask();

    void InitialSync();

    void CheckTimeout();

    void InitScheduleTask();

    PeerPtr GetSyncPeer();

    void PrintConnectedPeers();

    /*
     * default network parameter based on the protocol
     */

    // possibility of relaying a block to a peer
    constexpr static float kAlpha = 0.16;

    // max times a block is relayed with probability 1
    constexpr static uint8_t kMaxCountDown = 4;

    // max number of peers to whom we broadcast blocks
    constexpr static uint8_t kMaxPeerToBroadcast = 8;

    // max size of peers to relay address message
    const static uint32_t kMaxPeersToRelayAddr = 2;

    // max outbound size
    const size_t kMaxOutbound = 8;

    // The default timeout between when a connection attempt begins and version
    // message exchange completes
    const static int kConnectionSetupTimeout = 3 * 60;

    // broadcast local address per 24h
    const static uint32_t kBroadLocalAddressInterval = 24 * 60 * 60;

    // send addresses to neighbors per 30s
    const static uint32_t kSendAddressInterval = 30;

    const static uint32_t kCheckTimeoutInterval = 1; // second

    // interval of sending ping
    const static uint32_t kPingSendInterval = 2 * 60; // second

    // timeout between sending ping and receiving pong
    const static long kPingWaitTimeout = 3 * 60;

    // max number of ping failures
    const static size_t kMaxPingFailures = 3;

    constexpr static uint32_t kSyncTimeThreshold = 60;

    constexpr static uint32_t kCheckSyncInterval = 10;

    /**
     * my own peer id, a random number used to identify peer
     */

    uint64_t myID_;

    /*
     * current local network status
     */

    Scheduler scheduler_;

    /*
     * internal data structures
     */

    // peers' lock
    std::shared_mutex peerLock_;

    // a map to save all peers
    std::unordered_map<shared_connection_t, PeerPtr> peerMap_;

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

    std::thread initialSyncTask_;

    std::atomic_bool initial_sync_ = true;

    PeerPtr initial_sync_peer_ = nullptr;

    std::string connect_;

    // random generator
    std::default_random_engine gen;
};

extern std::unique_ptr<PeerManager> PEERMAN;
#endif // EPIC_PEER_MANAGER_H
