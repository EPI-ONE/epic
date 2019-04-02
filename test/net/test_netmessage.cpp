#include <gtest/gtest.h>
#include <ostream>
#include <vector>

#include "address_message.h"
#include "getaddr_message.h"
#include "ping.h"
#include "pong.h"
#include "version_ack.h"
#include "version_message.h"
#include "net_message.h"

class TestNetMsg : public testing::Test {
    public:
        NetAddress a1 = *NetAddress::StringToNetAddress("127.0.0.1:7877");
        NetAddress a2 = *NetAddress::StringToNetAddress("127.0.0.1:8245");
};

TEST_F(TestNetMsg, Ping) {
    Ping ping(7877);
    std::stringstream os;
    ping.Serialize(os);

    Ping ping1;
    ping1.Unserialize(os);
    EXPECT_EQ(ping.nonce_, ping1.nonce_);
}

TEST(TestNetMsg, Pong) {
    Pong pong(8245);
    std::stringstream os;
    pong.Serialize(os);

    Pong pong1;
    pong1.Unserialize(os);
    EXPECT_EQ(pong.nonce_, pong1.nonce_);
}

TEST_F(TestNetMsg, AddressMessage) {
    std::vector<NetAddress> lists{a1, a2};
    AddressMessage addressMessage(std::move(lists));
    std::stringstream os;
    addressMessage.Serialize(os);

    AddressMessage addressMessage1;
    addressMessage1.Unserialize(os);
    EXPECT_EQ(addressMessage.address_list_.size(), addressMessage1.address_list_.size());
    for (int i = 0; i < addressMessage.address_list_.size(); i++) {
        EXPECT_EQ(addressMessage.address_list_[i], addressMessage1.address_list_[i]);
    }
}

TEST_F(TestNetMsg, VersionMessage) {
    VersionMessage versionMessage(1, 0, time(0), a1, a2, 0);
    std::stringstream os;
    versionMessage.Serialize(os);

    VersionMessage versionMessage1;
    versionMessage1.Unserialize(os);
    EXPECT_EQ(versionMessage.current_height_,versionMessage1.current_height_);
    EXPECT_EQ(versionMessage.address_me_,versionMessage1.address_me_);
    EXPECT_EQ(versionMessage.address_you_,versionMessage1.address_you_);
    EXPECT_EQ(versionMessage.nTime_,versionMessage1.nTime_);
    EXPECT_EQ(versionMessage.local_service_,versionMessage1.local_service_);
    EXPECT_EQ(versionMessage.client_version_,versionMessage1.client_version_);

}