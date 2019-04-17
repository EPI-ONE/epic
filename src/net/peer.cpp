#include "peer.h"

Peer::Peer(const NetAddress &netAddress, const void *handle, bool inbound, bool isSeedPeer) :
    address(netAddress),
    connection_handle(handle),
    isInbound(inbound),
    isSeed(isSeedPeer) {
}

Peer::~Peer() {
    addrSendQueue.Quit();
}
