#ifndef EPIC_PEER_H
#define EPIC_PEER_H

#include <atomic>
#include <shared_mutex>

#include "address_manager.h"
#include "address_message.h"
#include "block.h"
#include "blocking_queue.h"
#include "concurrent_container.h"
#include "connection_manager.h"
#include "net_address.h"
#include "ping.h"
#include "pong.h"
#include "protocol_exception.h"
#include "spdlog/spdlog.h"
#include "sync_messages.h"
#include "task.h"
#include "version_ack.h"
#include "version_message.h"

class Peer {
public:
    Peer(NetAddress& netAddress, shared_connection_t connection, bool isSeedPeer, AddressManager* addressManager);

    virtual ~Peer();

    void SetWeakPeer(std::shared_ptr<Peer>& peer) {
        weak_peer_ = peer;
    };

    void ProcessMessage(unique_message_t& message);

    void SendMessage(unique_message_t&& message);

    /**
     * regularly send ping to the peer
     */
    void SendPing();

    /**
     * regularly send addresses to the peer
     */
    void SendAddresses();

    void SendVersion(uint64_t height);

    void SendLocalAddress();

    void Disconnect();

    uint64_t GetLastPingTime() {
        return lastPingTime;
    }

    size_t GetNPingFailed() const;

    void AddPendingGetInvTask(std::shared_ptr<GetInvTask> task);

    bool RemovePendingGetInvTask(uint32_t task_id);

    size_t GetInvTaskSize();

    void AddPendingGetDataTask(std::shared_ptr<GetDataTask> task) {
        getDataTasks.Push_back(task);
    }

    size_t GetDataTaskSize() {
        return getDataTasks.Size();
    }

    uint256 GetLastSentBundleHash() const {
        return lastSentBundleHash;
    }

    void SetLastSentBundleHash(const uint256& h) {
        lastSentBundleHash = h;
    }

    uint256 GetLastSentInvHash() const {
        return lastSentInvHash;
    }

    void SetLastSentInvHash(const uint256& h) {
        lastSentInvHash = h;
    }

    uint256 GetLastGetInvEnd() const {
        return lastGetInvEnd;
    }

    void SetLastGetInvEnd(const uint256& h) {
        lastGetInvEnd = h;
    }

    size_t GetLastGetInvLength() const {
        return lastGetInvLength;
    }

    void SetLastGetInvLength(const size_t& l) {
        lastGetInvLength = l;
    }

    bool IsInbound() {
        return connection_->IsInbound();
    }

    bool IsVaild() {
        return connection_->IsValid();
    }

    void StartSync();

    void CompleteSync() {
        isSyncing = false;
        isHigher  = false;
    }

    bool IsSyncTimeout();

    /*
     * basic information of peer
     */

    // network address
    const NetAddress address;

    // if the peer is a seed
    const bool isSeed;

    // the time when the connection is setup
    const uint64_t connected_time;

    // version message
    VersionMessage* versionMessage = nullptr;

    // a peer is fully connected when we receive his version message and version
    // ack
    std::atomic_bool isFullyConnected = false;

    std::atomic_bool isHigher = false;

    std::atomic_bool isSyncing = false;

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
    void ProcessVersionMessage(VersionMessage& version);

    /*
     * process version ack message
     */
    void ProcessVersionACK();

    /*
     * process address message, check, relay and save addresses
     * @param addressMessage
     */
    void ProcessAddressMessage(AddressMessage& addressMessage);

    /**
     * send addresses to the peer
     */
    void ProcessGetAddrMessage();

    /**
     * process transaction, add to memory pool and relay
     * @param transaction
     */
    void ProcessTransaction(const ConstTxPtr& tx);

    /**
     * process block, add to dag and relay
     * @param block
     */
    void ProcessBlock(const ConstBlockPtr& block);

    /**
     * process GetInv, respond with an Inv message
     */
    void ProcessGetInv(GetInv& getBlock);

    /**
     * process Inv, respond with a GetData message
     */
    void ProcessInv(std::unique_ptr<Inv> inv);

    /**
     * processGetData, respond with bundles
     */
    void ProcessGetData(GetData& getData);

    /**
     * process bundle, add to dag
     * @param bundle
     */
    void ProcessBundle(const std::shared_ptr<Bundle>& bundle);

    /**
     * process notfound, terminate synchronization and clear all the queues
     * @param nonce
     */
    void ProcessNotFound(const uint32_t& nonce);

    /**
     * Parameters of network setting
     */
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
    std::atomic_uint64_t lastPingTime = time(nullptr);
    std::atomic_uint64_t lastPongTime = time(nullptr);

    // last sending nonce
    std::atomic_uint64_t lastNonce;

    // number of ping failures
    size_t nPingFailed;

    // if we have reply GetAddr to this peer
    bool haveReplyGetAddr;

    // the address queue to send
    ConcurrentQueue<NetAddress> addrSendQueue;

    /*
     * Synchronization information
     */


    // Keep track of the last request we made to the peer in GetInv
    // so we can avoid redundant and harmful GetInv requests.
    std::atomic<uint256> lastGetInvEnd;
    std::atomic_size_t lastGetInvLength;
    std::atomic<uint256> lastSentBundleHash;
    std::atomic<uint256> lastSentInvHash;

    mutable std::shared_mutex inv_task_mutex_;
    std::unordered_map<uint32_t, std::shared_ptr<GetInvTask>> getInvsTasks;
    GetDataTaskManager getDataTasks;

    std::weak_ptr<Peer> weak_peer_;

    /*
     * pointer from outside
     */
    AddressManager* addressManager_;
    shared_connection_t connection_;
};

typedef std::shared_ptr<Peer> PeerPtr;

#endif // EPIC_PEER_H
