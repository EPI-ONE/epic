#include <gtest/gtest.h>
#include <ostream>
#include <vector>

#include "address_message.h"
#include "getaddr_message.h"
#include "net_message.h"
#include "ping.h"
#include "pong.h"
#include "version_ack.h"
#include "version_message.h"

class TestNetMsg : public testing::Test {
public:
    NetAddress a1 = *NetAddress::GetByIP("127.0.0.1:7877");
    NetAddress a2 = *NetAddress::GetByIP("127.0.0.1:8245");
};

TEST_F(TestNetMsg, Ping) {
    Ping ping(7877);
    std::stringstream os;
    ping.Serialize(os);

    Ping ping1;
    ping1.Deserialize(os);
    EXPECT_EQ(ping.nonce, ping1.nonce);
}

TEST_F(TestNetMsg, Pong) {
    Pong pong(8245);
    std::stringstream os;
    pong.Serialize(os);

    Pong pong1;
    pong1.Deserialize(os);
    EXPECT_EQ(pong.nonce, pong1.nonce);
}

TEST_F(TestNetMsg, AddressMessage) {
    std::vector<NetAddress> lists{a1, a2};
    AddressMessage addressMessage(lists);
    std::stringstream os;
    addressMessage.Serialize(os);

    AddressMessage addressMessage1;
    addressMessage1.Deserialize(os);
    EXPECT_EQ(addressMessage.addressList.size(), addressMessage1.addressList.size());
    for (size_t i = 0; i < addressMessage.addressList.size(); i++) {
        EXPECT_EQ(addressMessage.addressList[i], addressMessage1.addressList[i]);
    }
}

TEST_F(TestNetMsg, VersionMessage) {
    VersionMessage versionMessage(1, 0, time(0), a1, 0);
    std::stringstream os;
    versionMessage.Serialize(os);

    VersionMessage versionMessage1;
    versionMessage1.Deserialize(os);
    EXPECT_EQ(versionMessage.current_height, versionMessage1.current_height);
    EXPECT_EQ(versionMessage.address_you, versionMessage1.address_you);
    EXPECT_EQ(versionMessage.nTime, versionMessage1.nTime);
    EXPECT_EQ(versionMessage.local_service, versionMessage1.local_service);
    EXPECT_EQ(versionMessage.client_version, versionMessage1.client_version);
}
