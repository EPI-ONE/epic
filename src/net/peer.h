#ifndef EPIC_PEER_H
#define EPIC_PEER_H

#include <atomic>
#include "blocking_queue.h"
#include "net_address.h"
#include "version_message.h"

class Peer {
    public:
        /**
         * construct a peer called by peer manager
         * @param netAddress
         * @param handle , the libevent socket handle
         * @param inbound
         * @param isSeedPeer, if the peer address is a seed
         */
        Peer(NetAddress &netAddress, const void *handle, bool inbound, bool isSeedPeer);

        ~Peer();

        /**
        * basic information of peer
        */

        // network address
        const NetAddress address;

        // libevent connection handle
        const void *connection_handle;

        // if the peer is a seed
        const bool isSeed;

        // if the peer connects us first
        const bool isInbound;

        // version message
        VersionMessage *versionMessage = nullptr;

        // a peer is fully connected when we receive his version message and version ack
        std::atomic_bool isFullyConnected;

        // if we will disconnect the peer
        std::atomic_bool disconnect;

        /**
         * called when receive a ping message
         * @param nonce
         */
        void UpdatePingStatic(long nonce);

    private:

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

        // The default timeout between when a connection attempt begins and version message exchange completes
        const static int kConnectionSetupTimeout = 10 * 1000;

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


};

#endif //EPIC_PEER_H
