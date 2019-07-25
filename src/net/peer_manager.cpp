#include "peer_manager.h"
#include <dag_manager.h>

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
    InitScheduleTask();

    connectionManager_->RegisterNewConnectionCallback(
        std::bind(&PeerManager::OnConnectionCreated, this, std::placeholders::_1));
    connectionManager_->RegisterDeleteConnectionCallBack(
        std::bind(&PeerManager::OnConnectionClosed, this, std::placeholders::_1));

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
    connectionManager_->QuitQueue();

    if (handleMessageTask_.joinable()) {
        handleMessageTask_.join();
    }

    if (scheduleTask_.joinable()) {
        scheduleTask_.join();
    }

    if (openConnectionTask_.joinable()) {
        openConnectionTask_.join();
    }

    DisconnectAllPeer();
    ClearPeers();
    connectionManager_->Stop();
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

void PeerManager::OnConnectionCreated(shared_connection_t& connection) {
    std::optional<NetAddress> net_address = NetAddress::GetByIP(connection->GetRemote());

    auto peer = CreatePeer(connection, *net_address);

    AddPeer(connection, peer);
    spdlog::info("{} {}   ({} connected)", connection->IsInbound() ? "Accept" : "Connect to", connection->GetRemote(),
                 GetConnectedPeerSize());

    // send version message
    if (!peer->IsInbound()) {
        peer->SendVersion(DAG->GetBestMilestoneHeight());
    }
}

void PeerManager::OnConnectionClosed(shared_connection_t& connection) {
    std::thread t([connection, this]() { RemovePeer(connection); });
    t.detach();
}

void PeerManager::DisconnectAllPeer() {
    std::shared_lock<std::shared_mutex> lk(peerLock_);
    for (auto peer : peerMap_) {
        peer.second->Disconnect();
    }
}

PeerPtr PeerManager::CreatePeer(shared_connection_t& connection, NetAddress& address) {
    auto peer = std::make_shared<Peer>(address, connection, addressManager_->IsSeedAddress(address), addressManager_);
    peer->SetWeakPeer(peer);
    return peer;
}

void PeerManager::ClearPeers() {
    std::unique_lock<std::shared_mutex> lk(peerLock_);
    peerMap_.clear();
}

void PeerManager::RemovePeer(shared_connection_t connection) {
    std::unique_lock<std::shared_mutex> lk(peerLock_);
    peerMap_.erase(connection);
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
    std::shared_lock<std::shared_mutex> lk(peerLock_);
    return peerMap_.size();
}

size_t PeerManager::GetFullyConnectedPeerSize() {
    std::shared_lock<std::shared_mutex> lk(peerLock_);
    size_t count = 0;
    for (auto& it : peerMap_) {
        if (it.second->isFullyConnected) {
            ++count;
        }
    }
    return count;
}

void PeerManager::HandleMessage() {
    while (!interrupt_) {
        connection_message_t msg;
        if (connectionManager_->ReceiveMessage(msg)) {
            auto msg_from = GetPeer(msg.first);
            if (msg_from && msg_from->IsVaild()) {
                msg_from->ProcessMessage(msg.second);
            }
        }
    }
}

void PeerManager::OpenConnection() {
    while (!interrupt_) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        if (connectionManager_->GetOutboundNum() > kMax_outbound) {
            continue;
        }
        auto seed = addressManager_->GetOneSeed();
        if (seed) {
            NetAddress seed_address = NetAddress(*seed, CONFIG->defaultPort);
            ConnectTo(seed_address);
        }

        int num_tries = 0;
        while (num_tries < 100) {
            ++num_tries;
            auto try_to_connect = addressManager_->GetOneAddress(false);

            // means we don't have enough addresses to connect
            if (!try_to_connect) {
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
    std::unique_lock<std::shared_mutex> lk(peerLock_);
    for (auto it = peerMap_.begin(); it != peerMap_.end();) {
        std::shared_ptr<Peer> peer = it->second;
        if (peer->isFullyConnected) {
            // check ping timeout
            if (peer->GetLastPingTime() > peer->GetLastPongTime() + kPingWaitTimeout ||
                peer->GetNPingFailed() > kMaxPingFailures) {
                spdlog::info("[NET:disconnect]: fully connected peer {} ping timeout", peer->address.ToString());
                peer->Disconnect();
                peerMap_.erase(it++);
            } else {
                it++;
            }
        } else {
            // check version timeout
            if (peer->connected_time + kConnectionSetupTimeout < (uint64_t) time(nullptr)) {
                spdlog::info("[NET:disconnect]: non-fully connected peer {} version handshake timeout",
                             peer->address.ToString());
                peer->Disconnect();
                peerMap_.erase(it++);
            } else {
                it++;
            }
        }
    }
}

void PeerManager::ScheduleTask() {
    while (!interrupt_) {
        scheduler_.Loop();
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

PeerPtr PeerManager::GetPeer(shared_connection_t& connection) {
    std::shared_lock<std::shared_mutex> lk(peerLock_);
    auto it = peerMap_.find(connection);
    return it == peerMap_.end() ? nullptr : it->second;
}

void PeerManager::AddPeer(shared_connection_t& connection, const std::shared_ptr<Peer>& peer) {
    std::unique_lock<std::shared_mutex> lk(peerLock_);
    peerMap_.insert(std::make_pair(connection, peer));
}

bool PeerManager::HasConnectedTo(const IPAddress& address) {
    std::shared_lock<std::shared_mutex> lk(peerLock_);
    for (auto& peer : peerMap_) {
        if (0 == std::memcmp(peer.second->address.GetIP(), address.GetIP(), 16)) {
            return true;
        }
    }
    return false;
}

void PeerManager::RelayBlock(const ConstBlockPtr& block, const PeerPtr& msg_from) {
    std::shared_lock<std::shared_mutex> lk(peerLock_);
    if (peerMap_.empty()) {
        return;
    }

    for (auto& it : peerMap_) {
        if (it.second != msg_from) {
            it.second->SendMessage(std::make_unique<Block>(*block));
        }
    }
}

void PeerManager::InitScheduleTask() {
    scheduler_.AddPeriodTask(kCheckTimeoutInterval, std::bind(&PeerManager::CheckTimeout, this));

    scheduler_.AddPeriodTask(kBroadLocalAddressInterval, [this]() {
        std::shared_lock<std::shared_mutex> lk(peerLock_);
        for (auto& it : peerMap_) {
            it.second->SendLocalAddress();
        }
    });

    scheduler_.AddPeriodTask(kSendAddressInterval, [this]() {
        std::shared_lock<std::shared_mutex> lk(peerLock_);
        for (auto& it : peerMap_) {
            it.second->SendAddresses();
        }
    });

    scheduler_.AddPeriodTask(kPingSendInterval, [this]() {
        std::shared_lock<std::shared_mutex> lk(peerLock_);
        for (auto& it : peerMap_) {
            it.second->SendPing();
        }
    });
}
