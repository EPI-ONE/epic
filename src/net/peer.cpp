#include "peer.h"

#include "caterpillar.h"
#include "mempool.h"
#include "peer_manager.h"

Peer::Peer(NetAddress& netAddress, shared_connection_t connection, bool isSeedPeer, AddressManager* addressManager)
    : address(std::move(netAddress)), isSeed(isSeedPeer), connected_time(time(nullptr)), lastPingTime(0),
      lastPongTime(0), nPingFailed(0), addressManager_(addressManager), connection_(connection) {}

Peer::~Peer() {
    delete versionMessage;
}

void Peer::ProcessMessage(unique_message_t& msg) {
    try {
        switch (msg->GetType()) {
            case NetMessage::PING: {
                ProcessPing(*dynamic_cast<Ping*>(msg.get()));
                break;
            }
            case NetMessage::PONG: {
                ProcessPong(*dynamic_cast<Pong*>(msg.get()));
                break;
            }
            case NetMessage::VERSION_MSG: {
                ProcessVersionMessage(*dynamic_cast<VersionMessage*>(msg.get()));
                break;
            }
            case NetMessage::VERSION_ACK: {
                ProcessVersionACK();
                break;
            }
            case NetMessage::GET_ADDR: {
                ProcessGetAddrMessage();
                break;
            }
            case NetMessage::ADDR: {
                ProcessAddressMessage(*dynamic_cast<AddressMessage*>(msg.get()));
                break;
            }
            case NetMessage::GET_INV: {
                ProcessGetInv(*dynamic_cast<GetInv*>(msg.get()));
                break;
            }
            case NetMessage::INV: {
                ProcessInv(std::unique_ptr<Inv>(dynamic_cast<Inv*>(msg.release())));
                break;
            }
            case NetMessage::GET_DATA: {
                ProcessGetData(*dynamic_cast<GetData*>(msg.get()));
                break;
            }
            case NetMessage::BUNDLE: {
                ProcessBundle(std::shared_ptr<Bundle>(dynamic_cast<Bundle*>(msg.release())));
                break;
            }
            case NetMessage::TX: {
                ProcessTransaction(std::shared_ptr<Transaction>(dynamic_cast<Transaction*>(msg.release())));
                break;
            }
            case NetMessage::BLOCK: {
                Block* b  = dynamic_cast<Block*>(msg.release());
                b->source = Block::NETWORK;
                ProcessBlock(std::shared_ptr<const Block>(b));
                break;
            }
            case NetMessage::NOT_FOUND: {
                NotFound* notfound = dynamic_cast<NotFound*>(msg.get());
                spdlog::warn("Not found: {}", notfound->hash.to_substr());
                ProcessNotFound(notfound->nonce);
                break;
            }
            default: {
                throw ProtocolException("undefined message");
            }
        }
    } catch (ProtocolException& exception) {
        spdlog::debug(exception.ToString());
    } catch (const std::bad_cast& e) {
        spdlog::debug("message cast fail {}", e.what());
    }
}

void Peer::ProcessVersionACK() {
    if (isFullyConnected) {
        spdlog::warn("{}: VersionMessage is null before processing VersionAck", address.ToString());
        throw ProtocolException("got more than one version ack");
    }
    isFullyConnected = true;
    spdlog::info("finish version handshake with {}", address.ToString());

    if (IsInbound()) {
        if (versionMessage->current_height > DAG->GetBestMilestoneHeight()) {
            spdlog::info("we are behind our peer {}, start batch synchronization", address.ToString());
            DAG->RequestInv(uint256(), 5, weak_peer_.lock());
        }
    }
}

void Peer::ProcessPing(const Ping& ping) {
    SendMessage(std::make_unique<Pong>(ping.nonce));
}

void Peer::ProcessPong(const Pong& pong) {
    lastPongTime = time(nullptr);
    nPingFailed  = pong.nonce == lastNonce ? 0 : nPingFailed + 1;
    spdlog::info("receive pong from {}, nonce = {}", address.ToString(), pong.nonce);
}

void Peer::ProcessVersionMessage(VersionMessage& versionMessage_) {
    if (versionMessage) {
        throw ProtocolException("Got two version messages from peer");
    }

    // TODO check if we have connected to ourself

    // check version
    if (versionMessage_.client_version < kMinProtocolVersion) {
        spdlog::warn("client version {} < min protocol version {}, disconnect peer {}", versionMessage_.client_version,
                     kMinProtocolVersion, address.ToString());
        Disconnect();
        return;
    }

    versionMessage  = new VersionMessage();
    *versionMessage = versionMessage_;
    char time_buffer[20];
    strftime(time_buffer, 20, "%Y-%m-%d %H:%M:%S", localtime((time_t*) &(versionMessage->nTime)));
    spdlog::info("{}: Got version = {}, services = {}, time = {}, height = {}", address.ToString(),
                 versionMessage->client_version, versionMessage->local_service, std::string(time_buffer),
                 versionMessage->current_height);

    // send version message if peer is inbound
    if (IsInbound()) {
        SendVersion(DAG->GetBestMilestoneHeight());
    }

    // send version ack
    SendMessage(std::make_unique<NetMessage>(NetMessage::VERSION_ACK));

    // add the score of the our local address as reported by the peer
    addressManager_->SeenLocalAddress(versionMessage_.address_you);
    if (!IsInbound()) {
        // send local address
        SendLocalAddress();

        // ask for addresses
        SendMessage(std::make_unique<NetMessage>(NetMessage::GET_ADDR));

        addressManager_->MarkOld(address);

        if (versionMessage->current_height > DAG->GetBestMilestoneHeight()) {
            spdlog::info("we are behind our peer {}, start batch synchronization", address.ToString());
            DAG->RequestInv(uint256(), 5, weak_peer_.lock());
        }
    }
}

void Peer::ProcessAddressMessage(AddressMessage& addressMessage) {
    if (addressMessage.addressList.size() > AddressMessage::kMaxAddressSize) {
        spdlog::warn("Received too many addresses, abort them");
    } else {
        spdlog::info("Received addresses from peer {}", address.ToString());
        for (const auto& addr : addressMessage.addressList) {
            spdlog::info("Received address {} ", addr.ToString());
            // TODO relay

            // save addresses
            if (addr.IsRoutable()) {
                addressManager_->AddNewAddress(addr);
            }
        }
    }

    // disconnect the connection after we get the addresses if the peer is a seed
    if (isSeed) {
        spdlog::warn("disconnect seed {}", address.ToString());
        Disconnect();
    }
}

void Peer::ProcessGetAddrMessage() {
    if (!IsInbound() || haveReplyGetAddr) {
        return;
    }
    SendMessage(std::make_unique<AddressMessage>(addressManager_->GetAddresses()));
    haveReplyGetAddr = true;
}

const std::atomic_uint64_t& Peer::GetLastPingTime() const {
    return lastPingTime;
}

void Peer::SetLastPingTime(const uint64_t lastPingTime_) {
    Peer::lastPingTime = lastPingTime_;
}

const std::atomic_uint64_t& Peer::GetLastPongTime() const {
    return lastPongTime;
}

void Peer::SetLastPongTime(const uint64_t lastPongTime_) {
    Peer::lastPongTime = lastPongTime_;
}

size_t Peer::GetNPingFailed() const {
    return nPingFailed;
}

void Peer::SetNPingFailed(size_t nPingFailed_) {
    Peer::nPingFailed = nPingFailed_;
}

void Peer::SendPing() {
    if (isFullyConnected) {
        lastNonce = time(nullptr);
        SendMessage(std::make_unique<Ping>(lastNonce));
        spdlog::info("send ping to {}, nonce = {}", address.ToString(), lastNonce);
    }
}

void Peer::SendAddresses() {
    if (!addrSendQueue.empty()) {
        std::vector<NetAddress> addresses;

        while (!addrSendQueue.empty() && addresses.size() < AddressMessage::kMaxAddressSize) {
            addresses.push_back(addrSendQueue.front());
            addrSendQueue.pop_front();
        }

        SendMessage(std::make_unique<AddressMessage>(std::move(addresses)));
    }
}

void Peer::ProcessTransaction(const ConstTxPtr& tx) {
    if (!tx->Verify()) {
        return;
    }
    if (MEMPOOL->ReceiveTx(tx)) {
        PEERMAN->RelayTransaction(tx, weak_peer_.lock());
    }
}

void Peer::ProcessBlock(const ConstBlockPtr& block) {
    DAG->AddNewBlock(block, weak_peer_.lock());
}

void Peer::ProcessGetInv(GetInv& getInv) {
    size_t locator_size = getInv.locator.size();
    if (locator_size == 0) {
        throw ProtocolException("locator size = 0, msg from " + address.ToString());
    }
    spdlog::info("Received a GetInv request \n"
                 "   from   {}\n"
                 "   to     {}\n"
                 "   length {}",
                 std::to_string(getInv.locator[0]), std::to_string(getInv.locator[getInv.locator.size() - 1]),
                 getInv.locator.size());

    DAG->RespondRequestInv(getInv.locator, getInv.nonce, weak_peer_.lock());
}

void Peer::ProcessInv(std::unique_ptr<Inv> inv) {
    spdlog::debug("received inventory message, size = {}, from {} ", inv->hashes.size(), address.ToString());
    auto task = RemovePendingGetInvTask(inv->nonce);
    if (!task) {
        throw ProtocolException("unknown inv, nonce = " + std::to_string(inv->nonce));
    } else {
        spdlog::debug("Size of getInvsTasks = {}, removed successfully", GetInvTaskSize());
    }
    DAG->CallbackRequestInv(std::move(inv));
}

void Peer::ProcessGetData(GetData& getData) {
    if (getData.bundleNonce.empty()) {
        throw ProtocolException("GetData nonce size = 0, msg from " + address.ToString());
    }

    switch (getData.type) {
        case GetDataTask::PENDING_SET: {
            spdlog::debug("Received a GetData request for pending blocks from {}", address.ToString());
            DAG->RespondRequestPending(getData.bundleNonce[0], weak_peer_.lock());
            break;
        }
        case GetDataTask::LEVEL_SET: {
            spdlog::debug("Received a GetData request for stored blocks from {} with hash {}", address.ToString(),
                          getData.hashes.front().to_substr());
            DAG->RespondRequestLVS(getData.hashes, getData.bundleNonce, weak_peer_.lock());
            break;
        }
        default: {
            throw ProtocolException("Unknown GetData type, msg from " + address.ToString());
        }
    }
}

void Peer::ProcessBundle(const std::shared_ptr<Bundle>& bundle) {
    uint32_t firstNonce = GetFirstGetDataNonce();
    auto task           = RemovePendingGetDataTask(bundle->nonce);
    if (!task) {
        throw ProtocolException("unknown bundle, nonce = " + std::to_string(bundle->nonce) + ", msg from " +
                                address.ToString());
    }
    switch (task->type) {
        case GetDataTask::LEVEL_SET: {
            if (firstNonce == 0) {
                throw ProtocolException("receive a bundle that we don't need, msg from " + address.ToString());
            }
            spdlog::info("bundle nonce = {}, first nonce = {}", bundle->nonce, firstNonce);

            if (bundle->nonce == firstNonce) {
                spdlog::info("The Bundle answers a GetDataTask of type {}, add it to dag, "
                             "lvsPool size = {}, MSHash = {}",
                             task->type, orphanLvsPool.size(), std::to_string(bundle->blocks[0]->GetHash()));

                // Since we swap the first and the last block in a level set before we store
                // it to DB, to make the ms being the first block, it is likely that our peer
                // does the same thing. Thus to avoid piling too many blocks in OBC, we swap
                // them back here, so that blocks are in time order.
                std::swap(bundle->blocks.front(), bundle->blocks.back());

                for (auto& block : bundle->blocks) {
                    DAG->AddNewBlock(block, weak_peer_.lock());
                }

                uint32_t nextNonce = GetFirstGetDataNonce();
                if (nextNonce != 0) {
                    auto next = orphanLvsPool.find(nextNonce);
                    if (next != orphanLvsPool.end()) {
                        auto next_bundle = next->second;
                        orphanLvsPool.erase(next);
                        ProcessBundle(next_bundle);
                    }
                }
            } else {
                orphanLvsPool[bundle->nonce] = bundle;
                AddPendingGetDataTask(*task);
                spdlog::info("The Bundle answers a GetDataTask of type {}, wait for prev level set to be solidified, "
                             "lvsPool size = {}",
                             task->type, orphanLvsPool.size());
            }

            break;
        }
        case GetDataTask::PENDING_SET: {
            spdlog::info("The Bundle answers a GetDataTask of type {}, add it to dag", task->type);
            for (auto& block : bundle->blocks) {
                DAG->AddNewBlock(block, nullptr);
            }
            break;
        }
    }
}

void Peer::ProcessNotFound(const uint32_t& nonce) {
    std::unique_lock<std::shared_mutex> writer(sync_lock);
    getInvsTasks.clear();
    getDataTasks.clear();
    orphanLvsPool.clear();
    writer.unlock();

    DAG->DisconnectPeerSync(weak_peer_.lock());
}

uint32_t Peer::GetFirstGetDataNonce() {
    auto it = getDataTasks.begin();
    if (it != getDataTasks.end()) {
        return it->second.id;
    }
    return 0;
}

void Peer::AddPendingGetInvTask(const GetInvTask& task) {
    std::unique_lock<std::shared_mutex> writer(sync_lock);
    getInvsTasks.insert_or_assign(task.id, task);
}

std::optional<GetInvTask> Peer::RemovePendingGetInvTask(uint32_t task_id) {
    std::optional<GetInvTask> result;
    std::unique_lock<std::shared_mutex> writer(sync_lock);
    auto it = getInvsTasks.find(task_id);

    if (it != getInvsTasks.end()) {
        result = it->second;
        getInvsTasks.erase(it);
    }

    return result;
}

size_t Peer::GetInvTaskSize() {
    std::shared_lock<std::shared_mutex> reader(sync_lock);
    return getInvsTasks.size();
}

void Peer::AddPendingGetDataTask(const GetDataTask& task) {
    std::unique_lock<std::shared_mutex> writer(sync_lock);
    getDataTasks.insert_or_assign(task.id, task);
}

std::optional<GetDataTask> Peer::RemovePendingGetDataTask(uint32_t task_id) {
    std::optional<GetDataTask> result;
    std::unique_lock<std::shared_mutex> writer(sync_lock);
    auto it = getDataTasks.find(task_id);

    if (it != getDataTasks.end()) {
        result = it->second;
        getDataTasks.erase(it);
    }
    return result;
}

size_t Peer::GetDataTaskSize() {
    std::shared_lock<std::shared_mutex> reader(sync_lock);
    return getDataTasks.size();
}

uint256 Peer::GetLastSentBundleHash() const {
    std::shared_lock<std::shared_mutex> reader(sync_lock);
    return lastSentBundleHash;
}

void Peer::SetLastSentBundleHash(const uint256& h) {
    std::unique_lock<std::shared_mutex> writer(sync_lock);
    lastSentBundleHash = h;
}

uint256 Peer::GetLastSentInvHash() const {
    std::shared_lock<std::shared_mutex> reader(sync_lock);
    return lastSentInvHash;
}

void Peer::SetLastSentInvHash(const uint256& h) {
    std::unique_lock<std::shared_mutex> writer(sync_lock);
    lastSentInvHash = h;
}

uint256 Peer::GetLastGetInvBegin() const {
    std::shared_lock<std::shared_mutex> reader(sync_lock);
    return lastGetInvBegin;
}

void Peer::SetLastGetInvBegin(const uint256& h) {
    std::unique_lock<std::shared_mutex> writer(sync_lock);
    lastGetInvBegin = h;
}

uint256 Peer::GetLastGetInvEnd() const {
    std::shared_lock<std::shared_mutex> reader(sync_lock);
    return lastGetInvEnd;
}

void Peer::SetLastGetInvEnd(const uint256& h) {
    std::unique_lock<std::shared_mutex> writer(sync_lock);
    lastGetInvEnd = h;
}

size_t Peer::GetLastGetInvLength() const {
    return lastGetInvLength.load();
}

void Peer::SetLastGetInvLength(const size_t& l) {
    lastGetInvLength = l;
}
void Peer::SendMessage(unique_message_t&& message) {
    connection_->SendMessage(std::move(message));
}

void Peer::SendVersion(uint64_t height) {
    SendMessage(std::make_unique<VersionMessage>(address, height, 0, 0));
    spdlog::info("send version message to {}", address.ToString());
}

void Peer::SendLocalAddress() {
    IPAddress localAddress = addressManager_->GetBestLocalAddress();
    if (!localAddress.IsRoutable()) {
        return;
    }
    std::vector<NetAddress> addresses{NetAddress(localAddress, CONFIG->GetBindPort())};
    SendMessage(std::make_unique<AddressMessage>(std::move(addresses)));
    spdlog::info("send local address {} to {}", localAddress.ToString(), address.ToString());
}
