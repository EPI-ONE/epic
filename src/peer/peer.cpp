// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "peer.h"
#include "block_store.h"
#include "mempool.h"
#include "peer_manager.h"

Peer::Peer(NetAddress& netAddress, shared_connection_t connection, bool isSeedPeer, AddressManager* addressManager)
    : address(std::move(netAddress)), isSeed(isSeedPeer), connected_time(time(nullptr)), lastPingTime(time(nullptr)),
      lastPongTime(time(nullptr)), nPingFailed(0), addressManager_(addressManager), connection_(connection) {}

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
}

void Peer::ProcessPing(const Ping& ping) {
    lastPingTime = time(nullptr);
    SendMessage(std::make_unique<Pong>(ping.nonce));
}

void Peer::ProcessPong(const Pong& pong) {
    lastPongTime = time(nullptr);
    nPingFailed  = pong.nonce == lastNonce ? 0 : nPingFailed + 1;
    spdlog::info("receive pong from {}, nonce = {}", address.ToString(), pong.nonce);
}

void Peer::ProcessVersionMessage(VersionMessage& version) {
    if (versionMessage) {
        throw ProtocolException("Got two version messages from peer");
    }

    // TODO check if we have connected to ourself

    // check version
    if (version.client_version < kMinProtocolVersion) {
        spdlog::warn("client version {} < min protocol version {}, disconnect peer {}", version.client_version,
                     kMinProtocolVersion, address.ToString());
        Disconnect();
        return;
    }

    versionMessage  = new VersionMessage();
    *versionMessage = version;
    char time_buffer[20];
    strftime(time_buffer, 20, "%Y-%m-%d %H:%M:%S", localtime((time_t*) &(versionMessage->nTime)));
    spdlog::info("{}: Got version = {}, services = {}, time = {}, height = {}", address.ToString(),
                 versionMessage->client_version, versionMessage->local_service, std::string(time_buffer),
                 versionMessage->current_height);

    if (versionMessage->current_height > DAG->GetBestMilestoneHeight()) {
        isSyncAvailable = true;
    }

    // send version message if peer is inbound
    if (IsInbound()) {
        SendVersion(DAG->GetBestMilestoneHeight());
    }

    // send version ack
    SendMessage(std::make_unique<NetMessage>(NetMessage::VERSION_ACK));

    // add the score of the our local address as reported by the peer
    addressManager_->SeenLocalAddress(version.address_you);
    if (!IsInbound()) {
        // send local address
        SendLocalAddress();

        // ask for addresses
        SendMessage(std::make_unique<NetMessage>(NetMessage::GET_ADDR));

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

size_t Peer::GetNPingFailed() const {
    return nPingFailed;
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
    if (!InvTaskContains(inv->nonce)) {
        spdlog::debug("unknown inv, nonce = {}", inv->nonce);
        return;
    }

    DAG->CallbackRequestInv(std::move(inv), weak_peer_.lock());
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
    if (getDataTasks.Empty()) {
        spdlog::debug("no pending task");
        return;
    }

    if (!getDataTasks.CompleteTask(bundle)) {
        spdlog::debug("unknown bundle, nonce = {}, msg from{} ", std::to_string(bundle->nonce), address.ToString());
        return;
    }

    spdlog::debug("receive bundle nonce = {}, first nonce = {}", bundle->nonce, getDataTasks.Front()->nonce);

    while (getDataTasks.Front() && getDataTasks.Front()->bundle) {
        auto& front = getDataTasks.Front();
        if (front->type == GetDataTask::LEVEL_SET) {
            last_bundle_ms_time = front->bundle->blocks.front()->GetTime();
            std::swap(front->bundle->blocks.front(), front->bundle->blocks.back());
            for (auto& block : front->bundle->blocks) {
                DAG->AddNewBlock(block, weak_peer_.lock());
            }
            spdlog::info("receive levelset ms = {}", front->bundle->blocks.back()->GetHash().to_substr());
        } else if (front->type == GetDataTask::PENDING_SET) {
            for (auto& block : bundle->blocks) {
                DAG->AddNewBlock(block, nullptr);
            }
            spdlog::info("receive pending set");
        }

        getDataTasks.Pop();
    }
}

void Peer::ProcessNotFound(const uint32_t& nonce) {
    Disconnect();
}

void Peer::AddPendingGetInvTask(std::shared_ptr<GetInvTask> task) {
    std::unique_lock<std::shared_mutex> writer(inv_task_mutex_);
    getInvsTasks.insert_or_assign(task->nonce, task);
}

bool Peer::RemovePendingGetInvTask(uint32_t task_id) {
    std::unique_lock<std::shared_mutex> writer(inv_task_mutex_);
    return getInvsTasks.erase(task_id);
}

bool Peer::InvTaskContains(uint32_t task_id) {
    std::shared_lock<std::shared_mutex> reader(inv_task_mutex_);
    return getInvsTasks.find(task_id) != getInvsTasks.end();
}

bool Peer::InvTaskEmpty() {
    std::shared_lock<std::shared_mutex> reader(inv_task_mutex_);
    return getInvsTasks.empty();
}

size_t Peer::GetInvTaskSize() {
    std::shared_lock<std::shared_mutex> reader(inv_task_mutex_);
    return getInvsTasks.size();
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

void Peer::StartSync() {
    if (isSeed) {
        return;
    }

    if (getDataTasks.Empty() && InvTaskEmpty()) {
        spdlog::info("Syncing start");
        DAG->RequestInv(uint256(), 5, weak_peer_.lock());
    }
}

bool Peer::IsSyncTimeout() {
    if (getDataTasks.IsTimeout()) {
        return true;
    }

    std::shared_lock<std::shared_mutex> reader(inv_task_mutex_);
    for (auto& task : getInvsTasks) {
        if (task.second->IsTimeout()) {
            return true;
        }
    }
    return false;
}

void Peer::Disconnect() {
    connection_->Disconnect();
    for (auto& task : getDataTasks.GetTasks()) {
        DAG->EraseDownloading(task->hash);
    }
}
