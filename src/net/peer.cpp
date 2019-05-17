#include "peer.h"

Peer::Peer(NetAddress& netAddress,
    const void* handle,
    bool inbound,
    bool isSeedPeer,
    ConnectionManager* connectionManager,
    AddressManager* addressManager)
    : address(std::move(netAddress)), connection_handle(handle), isSeed(isSeedPeer), isInbound(inbound),
      connectionManager_(connectionManager), addressManager_(addressManager) {}

Peer::~Peer() {
    addrSendQueue.Quit();
    delete versionMessage;
}

void Peer::ProcessMessage(NetMessage& msg) {
    try {
        switch (msg.header.type) {
        case PING: {
            Ping ping;
            msg.payload >> ping;
            ProcessPing(ping);
            break;
        }
        case PONG: {
            Pong pong;
            msg.payload >> pong;
            ProcessPong(pong);
            break;
        }
        case VERSION_MSG: {
            VersionMessage versionMessage_;
            msg.payload >> versionMessage_;
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
            AddressMessage addressMessage;
            msg.payload >> addressMessage;
            ProcessAddressMessage(addressMessage);
            break;
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
        VersionMessage vmsg = VersionMessage::GetFakeVersionMessage();
        vmsg.address_you    = address;
        VStream stream;
        stream << vmsg;
        SendMessage(NetMessage(connection_handle, VERSION_MSG, stream));
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
        disconnect = true;
    }
}

void Peer::ProcessGetAddrMessage() {
    if (!isInbound || haveReplyGetAddr) {
        return;
    }
    std::vector<NetAddress> addresses = addressManager_->GetAddresses();
    AddressMessage msg(addresses);
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
