#include "peer_manager.h"

PeerManager::PeerManager() {
    connectionManager = new ConnectionManager();
    addressManager = new AddressManager();
}

PeerManager::~PeerManager() {
    delete connectionManager;
    delete addressManager;
}

void PeerManager::Start() {
    spdlog::info("Starting the Peer Manager...");
    connectionManager->RegisterNewConnectionCallback(std::bind(&PeerManager::OnConnectionCreated, this,
                                                               std::placeholders::_1,
                                                               std::placeholders::_2,
                                                               std::placeholders::_3));
    connectionManager->RegisterDeleteConnectionCallBack(std::bind(&PeerManager::OnConnectionClosed, this,
                                                                  std::placeholders::_1));
    connectionManager->Start();
}

void PeerManager::Stop() {
    spdlog::info("Stopping the Peer Manager...");
    connectionManager->Stop();
    for (auto peer_entry:peerMap_) {
        delete peer_entry.second;
    }
}

void PeerManager::OnConnectionCreated(void *connection_handle, std::string address, bool inbound) {
    std::optional<NetAddress> net_address = NetAddress::StringToNetAddress(address);
    assert(net_address);
    Peer *peer = CreatePeer(connection_handle, *net_address, inbound);
    {
        std::lock_guard<std::mutex> lk(peerLock);
        peerMap_.insert(std::make_pair(connection_handle, peer));
    }
    spdlog::info("{} {}   ({} connected)", inbound ? "Accept" : "Connect to", address, peerMap_.size());
    // TODO version handshake
}

void PeerManager::OnConnectionClosed(void *connection_handle) {
    Peer *p = nullptr;
    {
        std::lock_guard<std::mutex> lk(peerLock);
        auto peer = peerMap_.find(connection_handle)->second;
        if (peer) {
            p = peer;
            peerMap_.erase(connection_handle);
        }
    }
    if (p) {
        DeletePeer(p);
    }
}

Peer *PeerManager::CreatePeer(void *connection_handle, NetAddress address, bool inbound) {
    Peer *peer = new Peer(address, connection_handle, inbound, addressManager->isSeedAddress(address));
    return peer;
}

void PeerManager::DeletePeer(Peer *peer) {
    spdlog::info("{} Peer died,  ({} connected)", peer->address.ToString(), peerMap_.size());
    //TODO remove some tasks or data from peer
    delete peer;
}

bool PeerManager::Bind(NetAddress &bindAddress) {
    return connectionManager->Listen(bindAddress.GetPort(), bindAddress.GetIpInt()) == 0;
}

bool PeerManager::Bind(const std::string &bindAddress) {
    std::optional<NetAddress> opt = NetAddress::StringToNetAddress(bindAddress);
    return opt ? Bind(*opt) : false;
}

bool PeerManager::ConnectTo(NetAddress &connectTo) {
    return connectionManager->Connect(connectTo.GetIpInt(), connectTo.GetPort()) == 0;
}

bool PeerManager::ConnectTo(const std::string &connectTo) {
    std::optional<NetAddress> opt = NetAddress::StringToNetAddress(connectTo);
    return opt ? ConnectTo(*opt) : false;
}

size_t PeerManager::GetConnectedPeerSize() {
    std::lock_guard<std::mutex> lk(peerLock);
    return peerMap_.size();
}
