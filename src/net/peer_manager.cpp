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

    //    Bind(config->GetBindAddress() + ":" + std::to_string(config->GetBindPort()));

    connectionManager_->Start();

    handleMessageTask_ = std::thread(std::bind(&PeerManager::HandleMessage, this));
    scheduleTask_      = std::thread(std::bind(&PeerManager::ScheduleTask, this));
    if (connect_.empty()) {
        openConnectionTask_ = std::thread(std::bind(&PeerManager::OpenConnection, this));
    } else {
        ConnectTo(connect_);
    }
}

void PeerManager::Stop() {
    spdlog::info("Stopping the Peer Manager...");
    interrupt_ = true;

    connectionManager_->Stop();

    if (handleMessageTask_.joinable()) {
        handleMessageTask_.join();
    }

    if (scheduleTask_.joinable()) {
        scheduleTask_.detach();
    }

    if (openConnectionTask_.joinable()) {
        openConnectionTask_.detach();
    }
}

bool PeerManager::Init(std::unique_ptr<Config>& config) {
    if (!Bind(config->GetBindAddress())) {
        return false;
    }
    if (!Listen(config->GetBindPort())) {
        return false;
    }

    connect_ = config->GetConnect();
    return true;
}

void PeerManager::OnConnectionCreated(void* connection_handle, const std::string& address, bool inbound) {
    std::optional<NetAddress> net_address = NetAddress::GetByIP(address);

    // disconnect if we have connected to the same ip address
    if (HasConnectedTo(*net_address)) {
        connectionManager_->Disconnect(connection_handle);
        return;
    }

    auto peer = CreatePeer(connection_handle, *net_address, inbound);

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

void PeerManager::OnConnectionClosed(const void* connection_handle) {
    {
        std::unique_lock<std::recursive_mutex> lk(peerLock_);
        auto peer = GetPeer(connection_handle);
        if (peer) {
            RemoveAddr(peer->address);
            peerMap_.erase(connection_handle);
        }
    }
}

void PeerManager::DisconnectPeer(std::shared_ptr<Peer>& peer) {
    peer->disconnect = true;
    connectionManager_->Disconnect(peer->connection_handle);
}

std::shared_ptr<Peer> PeerManager::CreatePeer(void* connection_handle, NetAddress& address, bool inbound) {
    return std::make_shared<Peer>(address, connection_handle, inbound, addressManager_->IsSeedAddress(address),
        connectionManager_, addressManager_);
}

void PeerManager::RemovePeer(const void* connection_handle) {
    std::unique_lock<std::recursive_mutex> lk(peerLock_);
    peerMap_.erase(connection_handle);
}

void PeerManager::DeletePeer(const std::shared_ptr<Peer>& peer) {
    spdlog::info("{} Peer died", peer->address.ToString());
    // TODO remove some tasks or data from peer
}

bool PeerManager::Listen(uint16_t port) {
    return connectionManager_->Listen(port);
}

bool PeerManager::Bind(IPAddress& bindAddress) {
    return connectionManager_->Bind(bindAddress.GetIpInt());
}

bool PeerManager::Bind(const std::string& bindAddress) {
    std::optional<IPAddress> opt = IPAddress::GetByIP(bindAddress);
    return opt ? Bind(*opt) : false;
}

bool PeerManager::ConnectTo(NetAddress& connectTo) {
    return connectionManager_->Connect(connectTo.GetIpInt(), connectTo.GetPort());
}

bool PeerManager::ConnectTo(const std::string& connectTo) {
    std::optional<NetAddress> opt = NetAddress::GetByIP(connectTo);
    return opt ? ConnectTo(*opt) : false;
}

size_t PeerManager::GetConnectedPeerSize() {
    std::unique_lock<std::recursive_mutex> lk(peerLock_);
    return peerMap_.size();
}

size_t PeerManager::GetFullyConnectedPeerSize() {
    std::unique_lock<std::recursive_mutex> lk(peerLock_);
    size_t count = 0;
    for (auto& it : peerMap_) {
        if (it.second->isFullyConnected) {
            ++count;
        }
    }
    return count;
}

void PeerManager::SendMessage(NetMessage& message) {
    connectionManager_->SendMessage(message);
}

void PeerManager::SendMessage(NetMessage&& message) {
    connectionManager_->SendMessage(message);
}

void PeerManager::HandleMessage() {
    while (!interrupt_) {
        NetMessage msg;
        if (connectionManager_->ReceiveMessage(msg)) {
            auto msg_from = GetPeer(msg.GetConnectionHandle());
            if (!msg_from) {
                spdlog::warn("can't find the peer with the handle {}", msg.GetConnectionHandle());
                continue;
            }
            (*msg_from)->ProcessMessage(msg);
        }
    }
}

void PeerManager::OpenConnection() {
    while (!interrupt_) {
        sleep(30);
        if (connectionManager_->GetOutboundNum() > kMax_outbound) {
            continue;
        }
        auto seed = addressManager_->GetOneSeed();
        if (seed) {
            NetAddress seed_address = NetAddress(*seed, config->defaultPort);
            ConnectTo(seed_address);
        }

        int num_tries = 0;
        while (num_tries < 100) {
            ++num_tries;
            auto try_to_connect = addressManager_->GetOneAddress(false);

            // means we don't have enough addresses to connect
            if (!try_to_connect) {
                std::this_thread::yield();
                break;
            }

            // check if we have connected to the address
            if (HasConnectedTo(*try_to_connect)) {
                continue;
            }

            uint64_t now     = time(nullptr);
            uint64_t lastTry = addressManager_->GetLastTry(*try_to_connect);

            // we don't try to connect to an address twice in 2 minutes
            if (now - lastTry < 120) {
                continue;
            }

            std::cout << "try to connect to " + try_to_connect->ToString();
            ConnectTo(*try_to_connect);
            addressManager_->SetLastTry(*try_to_connect, now);
            break;
        }
    }
}

void PeerManager::CheckTimeout() {
    std::unique_lock<std::recursive_mutex> lk(peerLock_);
    std::vector<const void*> remove_handles;
    for (auto& it : peerMap_) {
        std::shared_ptr<Peer> peer = it.second;
        if (peer->isFullyConnected) {
            // check ping timeout
            if (peer->GetLastPingTime() > peer->GetLastPongTime() + kPingWaitTimeout ||
                peer->GetNPingFailed() > kMaxPingFailures) {
                spdlog::info("[NET:disconnect]: fully connected peer {} ping timeout", peer->address.ToString());
                DisconnectPeer(peer);
                remove_handles.push_back(peer->connection_handle);
            }
        } else {
            // check version timeout
            if (peer->connected_time + kConnectionSetupTimeout < (uint64_t) time(nullptr)) {
                spdlog::info("[NET:disconnect]: non-fully connected peer {} version handshake timeout",
                    peer->address.ToString());
                DisconnectPeer(peer);
                remove_handles.push_back(peer->connection_handle);
            }
        }
    }

    for (auto& handle : remove_handles) {
        OnConnectionClosed(handle);
    }
    remove_handles.clear();
}

void PeerManager::BroadcastMessage() {
    std::unique_lock<std::recursive_mutex> lk(peerLock_);
    for (auto& it : peerMap_) {
        it.second->SendMessages();
    }
    SendLocalAddresses();
}

void PeerManager::ScheduleTask() {
    while (!interrupt_) {
        sleep(1 * 60);
        CheckTimeout();
        BroadcastMessage();
    }
}

std::optional<PeerPtr> PeerManager::GetPeer(const void* connection_handle) {
    std::unique_lock<std::recursive_mutex> lk(peerLock_);
    auto it = peerMap_.find(connection_handle);
    if (it == peerMap_.end()) {
        return {};
    }
    return it->second;
}

void PeerManager::AddPeer(const void* handle, const std::shared_ptr<Peer>& peer) {
    std::unique_lock<std::recursive_mutex> lk(peerLock_);
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

bool PeerManager::HasConnectedTo(const IPAddress& address) {
    std::unique_lock<std::mutex> lk(addressLock_);
    return connectedAddress_.find(address) != connectedAddress_.end();
}

void PeerManager::SendLocalAddresses() {
    if (lastSendLocalAddressTime + kBroadLocalAddressInterval >= time(nullptr)) {
        return;
    }

    auto local_address = addressManager_->GetBestLocalAddress();
    if (local_address.IsRoutable()) {
        AddressMessage msg = AddressMessage();
        msg.AddAddress(NetAddress(local_address, config->GetBindPort()));
        for (auto& it : peerMap_) {
            auto data = VStream(msg);
            NetMessage message(it.first, ADDR, data);
            SendMessage(message);
        }
    }
}

void PeerManager::CheckPendingPeers() {
    std::unique_lock<std::mutex> lk(addressLock_);
    for (auto it = pending_peers.begin(); it != pending_peers.end();) {
        if (it->second + 60 < time(nullptr)) {
            pending_peers.erase(it++);
        } else {
            it++;
        }
    }
}

void PeerManager::UpdatePendingPeers(IPAddress& address) {
    std::unique_lock<std::mutex> lk(addressLock_);
    auto it = pending_peers.find(address);
    if (it != pending_peers.end()) {
        pending_peers.erase(it);
    }
}

void PeerManager::RelayBlock(ConstBlockPtr block, PeerPtr msg_from) {
    std::unique_lock<std::recursive_mutex> lk(peerLock_);
    for (auto& it : peerMap_) {
        NetMessage msg(msg_from->connection_handle, BLOCK, VStream(block));
        it.second->SendMessage(msg);
    }
}
