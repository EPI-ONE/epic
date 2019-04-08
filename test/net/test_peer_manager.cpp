#include <gtest/gtest.h>
#include "peer_manager.h"

class TestPeerManager : public testing::Test {
    public:
        PeerManager server;
        PeerManager client;

        void SetUp() {
            server.Start();
            client.Start();
        }

        void TearDown() {
            server.Stop();
            client.Stop();
        }
};

TEST_F(TestPeerManager, CallBack) {
    EXPECT_TRUE(server.Bind("127.0.0.1:7877"));
    EXPECT_TRUE(client.ConnectTo("127.0.0.1:7877"));
    sleep(1);
    EXPECT_EQ(server.GetConnectedPeerSize(), 1);
    EXPECT_EQ(client.GetConnectedPeerSize(), 1);
}

TEST_F(TestPeerManager, CheckHaveConnectedSameIP) {
    EXPECT_TRUE(server.Bind("127.0.0.1:7877"));
    EXPECT_TRUE(client.ConnectTo("127.0.0.1:7877"));
    sleep(1);

    PeerManager same_ip_client;
    same_ip_client.Start();
    same_ip_client.ConnectTo("127.0.0.1:7877");
    sleep(1);
    EXPECT_EQ(server.GetConnectedPeerSize(), 1);
    EXPECT_EQ(same_ip_client.GetConnectedPeerSize(), 0);
    same_ip_client.Stop();
}

