#ifndef EPIC_PEER_H
#define EPIC_PEER_H

#include <atomic>

#include "address_manager.h"
#include "address_message.h"
#include "block.h"
#include "blocking_queue.h"
#include "bundle.h"
#include "connection_manager.h"
#include "getaddr_message.h"
#include "message_type.h"
#include "net_address.h"
#include "ping.h"
#include "pong.h"
#include "protocol_exception.h"
#include "spdlog/spdlog.h"
#include "version_ack.h"
#include "version_message.h"

class Peer {
public:
    /**
     *
     * @param netAddress
     * @param handle ,the libevent socket handle
     * @param inbound
     * @param isSeedPeer, if the peer address is a seed
     * @param connectionManager
     * @param addressManager
     */
    Peer(NetAddress& netAddress,
        const void* handle,
        bool inbound,
        bool isSeedPeer,
        ConnectionManager* connectionManager,
        AddressManager* addressManager);

    ~Peer();

    void ProcessMessage(NetMessage& message);

    void SendMessage(NetMessage& message);

    void SendMessage(NetMessage&& message);

    /**
     * basic information of peer
     */

    // network address
    const NetAddress address;

    // libevent connection handle
    const void* connection_handle;

    // if the peer is a seed
    const bool isSeed;

    // if the peer connects us first
    const bool isInbound;

    // version message
    VersionMessage* versionMessage = nullptr;

    // a peer is fully connected when we receive his version message and version
    // ack
    std::atomic_bool isFullyConnected;

    // if we will disconnect the peer
    std::atomic_bool disconnect;

private:
    /**
     * read the nonce and send back pong message
     * @param ping
     */
    void ProcessPing(const Ping& ping);

    /**
     * update ping statistic of the peer
     * @param pong
     */
    void ProcessPong(const Pong& pong);

    /**
     * process version message
     * @param versionMessage
     */
    void ProcessVersionMessage(VersionMessage& versionMessage_);

    /**
     * process version ack message
     */
    void ProcessVersionACK();

    /**
     * Parameters of network setting
     */

    // broadcast local address per 24h
    const static long kBroadLocalAddressInterval = 24 * 60 * 60;

    // send addresses to neighbors per 30s
    const static long kSendAddressInterval = 30;

    // timeout between sending ping and receiving pong
    const static long kPingWaitTimeout = 2 * 60;

    // interval of sending ping
    const static long kPingSendInterval = 5 * 60;

    // max number of ping failures
    const static size_t kMaxPingFailures = 3;

    // record at most 2000 net addresses
    const static int kMaxAddress = 2000;

    // The default timeout between when a connection attempt begins and version
    // message exchange completes
    const static int kConnectionSetupTimeout = 10 * 1000;

    // the lowest version number we're willing to accept. Lower than this will
    // result in an immediate disconnect
    const int kMinProtocolVersion = 0; // TODO to be set

    /**
     * statistic of peer status
     */

    // last sending ping time and last receiving pong time
    std::atomic_uint64_t lastPingTime, lastPongTime;

    // last sending nonce
    std::atomic_uint64_t lastNonce;

    // number of ping failures
    size_t nPingFailed;

    // last time of sending local address
    long lastSendLocalAddressTime;

    // last time of sending addresses
    long lastSendAddressTime;

    // if we have reply GetAddr to this peer
    bool haveReplyGetAddr;

    // the address queue to send
    BlockingQueue<NetAddress> addrSendQueue;


    /**
     * Synchronization information
     */
    // TODO

    /**
     * pointer from outside
     */

    ConnectionManager* connectionManager_;
    AddressManager* addressManager_;
};

#endif // EPIC_PEER_H
