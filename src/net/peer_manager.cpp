#include "peer_manager.h"

PeerManager::PeerManager() {
    connectionManager_ = new ConnectionManager();
    addressManager_    = new AddressManager();
}

PeerManager::~PeerManager() {
    delete connectionManager_;
    delete addressManager_;
}

void PeerManager::Start() {
    spdlog::info("Starting the Peer Manager...");
    addressManager_->Init();
    connectionManager_->RegisterNewConnectionCallback(std::bind(
        &PeerManager::OnConnectionCreated, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    connectionManager_->RegisterDeleteConnectionCallBack(
        std::bind(&PeerManager::OnConnectionClosed, this, std::placeholders::_1));
    connectionManager_->Start();
    handleMessageTask = std::thread(std::bind(&PeerManager::HandleMessage, this));
}

void PeerManager::Stop() {
    spdlog::info("Stopping the Peer Manager...");
    connectionManager_->Stop();
    interrupt = true;
    if (handleMessageTask.joinable()) {
        handleMessageTask.join();
    }
    std::unique_lock<std::mutex> lock(peerLock_);
    for (auto peer_entry : peerMap_) {
        delete peer_entry.second;
    }
    peerMap_.clear();
    connectedAddress_.clear();
}

void PeerManager::OnConnectionCreated(void* connection_handle, const std::string& address, bool inbound) {
    std::optional<NetAddress> net_address = NetAddress::StringToNetAddress(address);
    assert(net_address);
    // disconnect if we have connected to the same ip address
    if (HasConnectedTo(*net_address)) {
        connectionManager_->Disconnect(connection_handle);
        return;
    }

    Peer* peer = CreatePeer(connection_handle, *net_address, inbound);

    AddPeer(connection_handle, peer);
    AddAddr(peer->address);
    spdlog::info("{} {}   ({} connected)", inbound ? "Accept" : "Connect to", address, GetConnectedPeerSize());

    // send version message
    if (!peer->isInbound) {
        VersionMessage ver = VersionMessage::GetFakeVersionMessage();
        ver.address_you    = peer->address;
        VStream stream(ver);
        NetMessage msg(peer->connection_handle, VERSION_MSG, stream);
        SendMessage(msg);
        spdlog::info("send version message to {}", peer->address.ToString());
    }
}

void PeerManager::OnConnectionClosed(void* connection_handle) {
    Peer* p = nullptr;
    {
        std::unique_lock<std::mutex> lk(peerLock_);
        auto peer = peerMap_.find(connection_handle)->second;
        if (peer) {
            p = peer;
            peerMap_.erase(connection_handle);
        }
    }
    if (p) {
        RemoveAddr(p->address);
        DeletePeer(p);
    }
}

Peer* PeerManager::CreatePeer(void* connection_handle, NetAddress& address, bool inbound) {
    Peer* peer = new Peer(address, connection_handle, inbound, addressManager_->IsSeedAddress(address),
        connectionManager_, addressManager_);
    return peer;
}

void PeerManager::DeletePeer(Peer* peer) {
    spdlog::info("{} Peer died,  ({} connected)", peer->address.ToString(), GetConnectedPeerSize());
    // TODO remove some tasks or data from peer
    delete peer;
}

bool PeerManager::Bind(NetAddress& bindAddress) {
    return connectionManager_->Listen(bindAddress.GetPort(), bindAddress.GetIpInt()) == 0;
}

bool PeerManager::Bind(const std::string& bindAddress) {
    std::optional<NetAddress> opt = NetAddress::StringToNetAddress(bindAddress);
    return opt ? Bind(*opt) : false;
}

bool PeerManager::ConnectTo(NetAddress& connectTo) {
    return connectionManager_->Connect(connectTo.GetIpInt(), connectTo.GetPort()) == 0;
}

bool PeerManager::ConnectTo(const std::string& connectTo) {
    std::optional<NetAddress> opt = NetAddress::StringToNetAddress(connectTo);
    return opt ? ConnectTo(*opt) : false;
}

size_t PeerManager::GetConnectedPeerSize() {
    std::unique_lock<std::mutex> lk(peerLock_);
    return peerMap_.size();
}

bool PeerManager::HasConnectedTo(const IPAddress& address) {
    std::unique_lock<std::mutex> lk(addressLock_);
    return connectedAddress_.find(address) != connectedAddress_.end();
}

void PeerManager::SendMessage(NetMessage& message) {
    connectionManager_->SendMessage(message);
}

void PeerManager::SendMessage(NetMessage&& message) {
    connectionManager_->SendMessage(message);
}

void PeerManager::HandleMessage() {
    while (!interrupt) {
        NetMessage msg;
        if (connectionManager_->ReceiveMessage(msg)) {
            Peer* msg_from = GetPeer(msg.GetConnectionHandle());
            if (!msg_from) {
                spdlog::warn("can't find the peer with the handle {}", msg.GetConnectionHandle());
                continue;
            }
            msg_from->ProcessMessage(msg);
        }
    }
}

Peer* PeerManager::GetPeer(const void* connection_handle) {
    std::unique_lock<std::mutex> lk(peerLock_);
    auto it = peerMap_.find(connection_handle);
    return it == peerMap_.end() ? nullptr : it->second;
}

void PeerManager::AddPeer(const void* handle, Peer* peer) {
    std::unique_lock<std::mutex> lk(peerLock_);
    peerMap_.insert(std::make_pair(handle, peer));
}

void PeerManager::AddAddr(const NetAddress& address) {
    std::unique_lock<std::mutex> lk(addressLock_);
    connectedAddress_.insert(address);
}

void PeerManager::RemoveAddr(const NetAddress& address) {
    std::unique_lock<std::mutex> lk(addressLock_);
    connectedAddress_.erase(address);
}
