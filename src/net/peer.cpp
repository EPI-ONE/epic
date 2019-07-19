#include "peer.h"
#include "caterpillar.h"
#include "peer_manager.h"

Peer::Peer(NetAddress& netAddress,
           const void* handle,
           bool inbound,
           bool isSeedPeer,
           ConnectionManager* connectionManager,
           AddressManager* addressManager)
    : address(std::move(netAddress)), connection_handle(handle), isSeed(isSeedPeer), isInbound(inbound),
      connected_time(time(nullptr)), lastPingTime(0), lastPongTime(0), nPingFailed(0),
      connectionManager_(connectionManager), addressManager_(addressManager) {}

Peer::~Peer() {
    addrSendQueue.Quit();
    delete versionMessage;
}

void Peer::ProcessMessage(NetMessage& msg) {
    try {
        switch (msg.header.type) {
            case PING: {
                Ping ping(msg.payload);
                ProcessPing(ping);
                break;
            }
            case PONG: {
                Pong pong(msg.payload);
                ProcessPong(pong);
                break;
            }
            case VERSION_MSG: {
                VersionMessage versionMessage_(msg.payload);
                ProcessVersionMessage(versionMessage_);
                break;
            }
            case VERSION_ACK: {
                ProcessVersionACK();
                break;
            }
            case GET_ADDR: {
                ProcessGetAddrMessage();
                break;
            }
            case ADDR: {
                AddressMessage addressMessage(msg.payload);
                ProcessAddressMessage(addressMessage);
                break;
            }
            case GET_INV: {
                GetInv getBlock(msg.payload);
                ProcessGetInv(getBlock);
                break;
            }
            case INV: {
                auto inv = std::make_unique<Inv>(msg.payload);
                ProcessInv(std::move(inv));
                break;
            }
            case GET_DATA: {
                GetData getData(msg.payload);
                ProcessGetData(getData);
                break;
            }
            case BUNDLE: {
                auto bundle = std::make_shared<Bundle>(msg.payload);
                ProcessBundle(bundle);
                break;
            }
            case BLOCK: {
                Block b(msg.payload);
                b.source            = Block::NETWORK;
                ConstBlockPtr block = std::make_shared<const Block>(std::move(b));
                ProcessBlock(block);
                break;
            }
            default: {
                throw ProtocolException("undefined message");
            }
        }
    } catch (ProtocolException& exception) {
        spdlog::warn(exception.ToString());
    }
}

void Peer::ProcessVersionACK() {
    if (!versionMessage) {
        spdlog::warn("{}: VersionMessage is null before processing VersionAck", address.ToString());
        throw ProtocolException("got a version ack before version");
    }
    if (isFullyConnected) {
        spdlog::warn("{}: VersionMessage is null before processing VersionAck", address.ToString());
        throw ProtocolException("got more than one version ack");
    }
    isFullyConnected = true;
    spdlog::info("finish version handshake with {}", address.ToString());

    if (versionMessage->current_height > DAG->GetBestMilestoneHeight()) {
        spdlog::info("we are behind our peer {}, start batch synchronization", address.ToString());
        DAG->RequestInv(uint256(), 5, peerManager->GetPeer(connection_handle));
    }
}

void Peer::ProcessPing(const Ping& ping) {
    Pong pong(ping.nonce);
    VStream stream;
    stream << pong;
    NetMessage netMessage(connection_handle, PONG, stream);
    SendMessage(netMessage);
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
        disconnect = true;
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
    if (isInbound) {
        VersionMessage vmsg(address, DAG->GetBestMilestoneHeight());
        SendMessage(NetMessage(connection_handle, VERSION_MSG, VStream(vmsg)));
        spdlog::info("send version message to {}", address.ToString());
    }

    // send version ack
    SendMessage(NetMessage(connection_handle, VERSION_ACK, VStream()));

    // add the score of the our local address as reported by the peer
    addressManager_->SeenLocalAddress(versionMessage_.address_you);
    if (!isInbound) {
        // send local address
        AddressMessage addressMessage;
        IPAddress localAddress = addressManager_->GetBestLocalAddress();
        addressMessage.AddAddress(NetAddress(localAddress, config->GetBindPort()));
        SendMessage(NetMessage(connection_handle, ADDR, VStream(addressMessage)));
        spdlog::info("send local address {} to {}", localAddress.ToString(), address.ToString());

        // ask for addresses
        SendMessage(NetMessage(connection_handle, GET_ADDR, VStream()));

        addressManager_->MarkOld(address);
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
        disconnect = true;
    }
}

void Peer::ProcessGetAddrMessage() {
    if (!isInbound || haveReplyGetAddr) {
        return;
    }
    AddressMessage msg(addressManager_->GetAddresses());
    SendMessage(NetMessage(connection_handle, ADDR, VStream(msg)));
    haveReplyGetAddr = true;
}

void Peer::SendMessage(NetMessage& message) {
    try {
        connectionManager_->SendMessage(message);
    } catch (std::exception& e) {
        spdlog::warn("{} , failed  to send message to {}, disconnect", e.what(), address.ToString());
        connectionManager_->Disconnect(connection_handle);
    }
}

void Peer::SendMessage(NetMessage&& message) {
    try {
        connectionManager_->SendMessage(message);
    } catch (std::exception& e) {
        spdlog::warn("{} , failed  to send message to {}, disconnect", e.what(), address.ToString());
        connectionManager_->Disconnect(connection_handle);
    }
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

void Peer::SendMessages() {
    SendAddresses();
    if (isFullyConnected) {
        SendPing();
    }
}

void Peer::SendPing() {
    if (lastPingTime + kPingSendInterval < (uint64_t) time(nullptr)) {
        lastNonce = time(nullptr);
        Ping ping(lastNonce);
        NetMessage msg(connection_handle, PING, VStream(ping));
        SendMessage(msg);
        spdlog::info("send ping to {}, nonce = {}", address.ToString(), lastNonce);
    }
}

void Peer::SendAddresses() {
    if (lastSendAddressTime + kSendAddressInterval < (uint64_t) time(nullptr) && !addrSendQueue.Empty()) {
        AddressMessage msg;
        NetAddress addr;

        while (!addrSendQueue.Empty() && msg.addressList.size() < AddressMessage::kMaxAddressSize) {
            addrSendQueue.Take(addr);
            msg.AddAddress(addr);
        }

        SendMessage(NetMessage(connection_handle, ADDR, VStream(msg)));
        lastSendAddressTime = time(nullptr);
    }
}

void Peer::ProcessBlock(ConstBlockPtr& block) {
    DAG->AddNewBlock(block, peerManager->GetPeer(connection_handle));
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

    auto peer = peerManager->GetPeer(connection_handle);
    if (!peer) {
        return;
    }
    DAG->RespondRequestInv(getInv.locator, getInv.nonce, std::move(peer));
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
    auto peer = peerManager->GetPeer(connection_handle);
    if (!peer) {
        return;
    }

    if (getData.bundleNonce.empty()) {
        throw ProtocolException("GetData nonce size = 0, msg from " + address.ToString());
    }

    switch (getData.type) {
        case GetDataTask::PENDING_SET: {
            DAG->RespondRequestPending(getData.bundleNonce[0], peer);
            break;
        }
        case GetDataTask::LEVEL_SET: {
            DAG->RespondRequestLVS(getData.hashes, getData.bundleNonce, std::move(peer));
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
                    DAG->AddNewBlock(block, peerManager->GetPeer(connection_handle));
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
                DAG->AddNewBlock(block, peerManager->GetPeer(connection_handle));
            }
            break;
        }
    }
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
