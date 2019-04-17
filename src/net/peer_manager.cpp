#include "peer_manager.h"

PeerManager::PeerManager() : kMinProtocolVersion(0) {
    connectionManager_ = new ConnectionManager();
    addressManager_ = new AddressManager();
}

PeerManager::~PeerManager() {
    delete connectionManager_;
    delete addressManager_;
}

void PeerManager::Start() {
    spdlog::info("Starting the Peer Manager...");
    connectionManager_->RegisterNewConnectionCallback(std::bind(&PeerManager::OnConnectionCreated, this,
                                                                std::placeholders::_1,
                                                                std::placeholders::_2,
                                                                std::placeholders::_3));
    connectionManager_->RegisterDeleteConnectionCallBack(std::bind(&PeerManager::OnConnectionClosed, this,
                                                                   std::placeholders::_1));
    connectionManager_->Start();
    handleMessageTask = std::thread(std::bind(&PeerManager::HandleMessage, this));
}

void PeerManager::Stop() {
    spdlog::info("Stopping the Peer Manager...");
    connectionManager_->Stop();
    {
        std::lock_guard<std::mutex> lock(peerLock_);
        for (auto peer_entry:peerMap_) {
            delete peer_entry.second;
        }
    }
    interrupt = true;
    handleMessageTask.join();
}

void PeerManager::OnConnectionCreated(void *connection_handle, const std::string &address, bool inbound) {
    std::optional<NetAddress> net_address = NetAddress::StringToNetAddress(address);
    assert(net_address);
    // disconnect if we have connected to the same ip address
    if (HasConnectedTo(*net_address)) {
        connectionManager_->Disconnect(connection_handle);
        return;
    }

    Peer *peer = CreatePeer(connection_handle, *net_address, inbound);
    {
        std::lock_guard<std::mutex> lk(peerLock_);
        peerMap_.insert(std::make_pair(connection_handle, peer));
    }

    {
        std::lock_guard<std::mutex> lk(addressLock_);
        connectedAddress_.insert(peer->address);
    }

    spdlog::info("{} {}   ({} connected)", inbound ? "Accept" : "Connect to", address, GetConnectedPeerSize());

    // send version message
    if (!peer->isInbound) {
        VersionMessage ver = GetFakeVersionMessage();
        ver.address_you = peer->address;
        VStream stream(ver);
        NetMessage msg(peer->connection_handle, VERSION_MSG, stream);
        SendMessage(msg);
        spdlog::info("send version message to {}", peer->address.ToString());
    }
}

void PeerManager::OnConnectionClosed(void *connection_handle) {
    Peer *p = nullptr;
    {
        std::lock_guard<std::mutex> lk(peerLock_);
        auto peer = peerMap_.find(connection_handle)->second;
        if (peer) {
            p = peer;
            peerMap_.erase(connection_handle);
        }
    }
    if (p) {
        DeletePeer(p);
        std::lock_guard<std::mutex> lk(addressLock_);
        connectedAddress_.erase(p->address);
    }

}

Peer *PeerManager::CreatePeer(void *connection_handle, NetAddress &address, bool inbound) {
    Peer *peer = new Peer(address, connection_handle, inbound, addressManager_->isSeedAddress(address));
    return peer;
}

void PeerManager::DeletePeer(Peer *peer) {
    spdlog::info("{} Peer died,  ({} connected)", peer->address.ToString(), GetConnectedPeerSize());
    //TODO remove some tasks or data from peer
    delete peer;
}

bool PeerManager::Bind(NetAddress &bindAddress) {
    return connectionManager_->Listen(bindAddress.GetPort(), bindAddress.GetIpInt()) == 0;
}

bool PeerManager::Bind(const std::string &bindAddress) {
    std::optional<NetAddress> opt = NetAddress::StringToNetAddress(bindAddress);
    return opt ? Bind(*opt) : false;
}

bool PeerManager::ConnectTo(NetAddress &connectTo) {
    return connectionManager_->Connect(connectTo.GetIpInt(), connectTo.GetPort()) == 0;
}

bool PeerManager::ConnectTo(const std::string &connectTo) {
    std::optional<NetAddress> opt = NetAddress::StringToNetAddress(connectTo);
    return opt ? ConnectTo(*opt) : false;
}

size_t PeerManager::GetConnectedPeerSize() {
    std::lock_guard<std::mutex> lk(peerLock_);
    return peerMap_.size();
}

bool PeerManager::HasConnectedTo(const IPAddress &address) {
    std::lock_guard<std::mutex> lk(addressLock_);
    return connectedAddress_.find(address) != connectedAddress_.end();
}

void PeerManager::SendMessage(NetMessage &message) {
    connectionManager_->SendMessage(message);
}

void PeerManager::SendMessage(NetMessage &&message) {
    connectionManager_->SendMessage(message);
}

void PeerManager::HandleMessage() {
    while (!interrupt) {
        NetMessage msg;
        try {
            if (connectionManager_->ReceiveMessage(msg)) {
                Peer *msg_from = GetPeer(msg.getConnectionHandle());
                if (!msg_from) {
                    spdlog::warn("can't find the peer with the handle {}", msg.getConnectionHandle());
                    continue;
                }
                switch (msg.header.type) {
                    case PING: {
                        Ping ping;
                        msg.payload >> ping;
                        ProcessPing(ping, msg_from);
                        break;
                    }
                    case PONG: {
                        Pong pong;
                        msg.payload >> pong;
                        ProcessPong(pong, msg_from);
                        break;
                    }
                    case VERSION_MSG: {
                        VersionMessage versionMessage;
                        msg.payload >> versionMessage;
                        ProcessVersionMessage(versionMessage, msg_from);
                        break;
                    }
                    case VERSION_ACK: {
                        ProcessVersionACK(msg_from);
                        break;
                    }
                }
            }
        } catch (ProtocolException &e) {
            spdlog::error(e.what());
        }
    }
}

Peer *PeerManager::GetPeer(const void *connection_handle) {
    std::lock_guard<std::mutex> lk(peerLock_);
    auto it = peerMap_.find(connection_handle);
    return it == peerMap_.end() ? nullptr : it->second;
}

void PeerManager::ProcessPing(const Ping &ping, Peer *from) {
    Pong pong(ping.nonce);
    VStream stream;
    stream << pong;
    NetMessage netMessage(from->connection_handle, PONG, stream);
    SendMessage(netMessage);
}

void PeerManager::ProcessPong(const Pong &pong, Peer *from) {
    from->UpdatePingStatic(pong.nonce);
}

VersionMessage PeerManager::GetFakeVersionMessage() {
    VersionMessage versionMessage;
    versionMessage.client_version = 0;
    versionMessage.current_height = 0;
    versionMessage.local_service = 0;
    versionMessage.nTime = time(nullptr);
    versionMessage.address_me = *NetAddress::StringToNetAddress("127.0.0.1:7877");
    return versionMessage;
}

void PeerManager::ProcessVersionMessage(VersionMessage &versionMessage, Peer *from) {
    if (from->versionMessage) {
        throw ProtocolException("Got two version messages from peer");
    }

    //TODO check if we have connected to ourself

    // check version
    if (versionMessage.client_version < kMinProtocolVersion) {
        spdlog::warn("client version {} < min protocol version {}, disconnect peer {}",
                     versionMessage.client_version,
                     kMinProtocolVersion,
                     from->address.ToString());
        from->disconnect = true;
    }

    from->versionMessage = new VersionMessage();
    *from->versionMessage = std::move(versionMessage);
    char time_buffer[20];
    strftime(time_buffer, 20, "%Y-%m-%d %H:%M:%S", localtime((time_t *) &(from->versionMessage->nTime)));
    spdlog::info("{}: Got version = {}, services = {}, time = {}, height = {}",
                 from->address.ToString(),
                 from->versionMessage->client_version,
                 from->versionMessage->local_service,
                 std::string(time_buffer),
                 from->versionMessage->current_height
    );

    // send version message if peer is inbound
    if (from->isInbound) {
        VersionMessage vmsg = GetFakeVersionMessage();
        vmsg.address_you = from->address;
        VStream stream;
        stream << vmsg;
        SendMessage(NetMessage(from->connection_handle, VERSION_MSG, stream));
        spdlog::info("send version message to {}", from->address.ToString());
    }

    // send version ack
    SendMessage(NetMessage(from->connection_handle, VERSION_ACK, VStream()));

    if (!from->isInbound) {
        //TODO handle address
    }
}

void PeerManager::ProcessVersionACK(Peer *from) {
    if (!from->versionMessage) {
        spdlog::warn("{}: VersionMessage is null before processing VersionAck", from->address.ToString());
        throw ProtocolException("got a version ack before version");
    }
    if (from->isFullyConnected) {
        spdlog::warn("{}: VersionMessage is null before processing VersionAck", from->address.ToString());
        throw ProtocolException("got more than one version ack");
    }
    from->isFullyConnected = true;
    spdlog::info("finish version handshake with {}", from->address.ToString());
}




