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
    /*
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
     * send scheduled messages(ping, address) to the peer
     */
    void SendMessages();

    /**
     * regularly send ping to the peer
     */
    void SendPing();

    /**
     * regularly send addresses to the peer
     */
    void SendAddresses();

    const std::atomic_uint64_t& GetLastPingTime() const;

    void SetLastPingTime(const uint64_t lastPingTime_);

    const std::atomic_uint64_t& GetLastPongTime() const;

    void SetLastPongTime(const uint64_t lastPongTime_);

    size_t GetNPingFailed() const;

    void SetNPingFailed(size_t nPingFailed_);

    /*
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

    // the time when the connection is setup
    const uint64_t connected_time;

    // version message
    VersionMessage* versionMessage = nullptr;

    // a peer is fully connected when we receive his version message and version
    // ack
    std::atomic_bool isFullyConnected;

    // if we will disconnect the peer
    std::atomic_bool disconnect;

private:
    /*
     * read the nonce and send back pong message
     * @param ping
     */
    void ProcessPing(const Ping& ping);

    /*
     * update ping statistic of the peer
     * @param pong
     */
    void ProcessPong(const Pong& pong);

    /*
     * process version message
     * @param versionMessage
     */
    void ProcessVersionMessage(VersionMessage& versionMessage_);

    /*
     * process version ack message
     */
    void ProcessVersionACK();

    /*
     * process address message, check, relay and save addresses
     * @param addressMessage
     */
    void ProcessAddressMessage(AddressMessage& addressMessage);

    /*
     * send addresses to the peer
     */
    void ProcessGetAddrMessage();

    /*
     * Parameters of network setting
     */

    // interval of sending ping
    const static uint64_t kPingSendInterval = 2 * 60;

    // send addresses to neighbors per 30s
    const static uint64_t kSendAddressInterval = 30;

    // record at most 2000 net addresses
    const static int kMaxAddress = 2000;

    // the lowest version number we're willing to accept. Lower than this will
    // result in an immediate disconnect
    // TODO to be set
    const int kMinProtocolVersion = 0;


    /*
     * statistic of peer status
     */

    // last sending ping time and last receiving pong time
    std::atomic_uint64_t lastPingTime, lastPongTime;

    // last sending nonce
    std::atomic_uint64_t lastNonce;

    // number of ping failures
    size_t nPingFailed;

    // last time of sending addresses
    long lastSendAddressTime;

    // if we have reply GetAddr to this peer
    bool haveReplyGetAddr;

    // the address queue to send
    BlockingQueue<NetAddress> addrSendQueue;


    /*
     * TODO: Synchronization information
     */

    /*
     * pointer from outside
     */
    ConnectionManager* connectionManager_;
    AddressManager* addressManager_;
};

#endif // EPIC_PEER_H
