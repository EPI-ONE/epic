#include "test_network.h"

void TestPeer::SendMessage(NetMessage&& message) {
    sentMsgBox.Put(message);
}

void TestPeer::SendMessage(NetMessage& message) {
    sentMsgBox.Put(message);
}

void TestPM::AddNewTestPeer(int id) {
    PeerPtr p = std::make_shared<TestPeer>(id);
    testPeers.insert_or_assign(id, p);
}

PeerPtr TestPM::GetPeer(const void* id) {
    auto it = testPeers.find(long(id));
    if (it != testPeers.end()) {
        return it->second;
    }
    return nullptr;
}
