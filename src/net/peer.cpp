#include "peer.h"

Peer::Peer(NetAddress &netAddress, const void *handle, bool inbound, bool isSeedPeer) :
    address(std::move(netAddress)),
    connection_handle(handle),
    isInbound(inbound),
    isSeed(isSeedPeer) {
}

Peer::~Peer() {
    addrSendQueue.Quit();
    delete versionMessage;
}

void Peer::UpdatePingStatic(long nonce) {
    lastPongTime = time(nullptr);
    nPingFailed = nonce == lastNonce ? 0 : nPingFailed + 1;
}
