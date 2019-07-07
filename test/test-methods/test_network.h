#ifndef EPIC_TEST_PEER_H
#define EPIC_TEST_PEER_H
#include "peer.h"
#include "peer_manager.h"

class TestPeer : public Peer {
public:
    static NetAddress& GetFakeAddr() {
        static NetAddress addr = *(NetAddress::GetByIP("127.0.0.1", 7877));
        return addr;
    }

    void SendMessage(NetMessage& message) override;
    void SendMessage(NetMessage&& message) override;

    explicit TestPeer(long id) : Peer(GetFakeAddr(), (const void*) (id), false, false, nullptr, nullptr) {}

    BlockingQueue<NetMessage> sentMsgBox;
};


class TestPM : public PeerManager {
public:
    void AddNewTestPeer(int id);

    PeerPtr GetPeer(const void* id) override;

protected:
    std::unordered_map<int, PeerPtr> testPeers;
};
#endif // EPIC_TEST_PEER_H
