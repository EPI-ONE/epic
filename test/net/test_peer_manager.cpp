
#include "peer_manager.h"
#include "test_env.h"
#include <gtest/gtest.h>

class TestPeerManager : public testing::Test {
public:
    PeerManager server;
    PeerManager client;

    static void SetUpTestCase() {
        CONFIG = std::make_unique<Config>();
        CONFIG->SetBindPort(43256);
        EpicTestEnvironment::SetUpDAG("test_peer_manager/");
    }
    static void TearDownTestCase() {
        EpicTestEnvironment::TearDownDAG("test_peer_manager/");
    }
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
    ASSERT_TRUE(server.Bind("127.0.0.1"));
    EXPECT_TRUE(server.Listen(43256));
    EXPECT_TRUE(client.ConnectTo("127.0.0.1:43256"));
    usleep(50000);
    EXPECT_EQ(server.GetFullyConnectedPeerSize(), 1);
    EXPECT_EQ(client.GetFullyConnectedPeerSize(), 1);
}

TEST_F(TestPeerManager, CheckHaveConnectedSameIP) {
    ASSERT_TRUE(server.Bind("127.0.0.1"));
    EXPECT_TRUE(server.Listen(43256));
    EXPECT_TRUE(client.ConnectTo("127.0.0.1:43256"));
    usleep(50000);

    PeerManager same_ip_client;
    same_ip_client.Start();
    same_ip_client.ConnectTo("127.0.0.1:43256");
    usleep(50000);
    EXPECT_EQ(server.GetFullyConnectedPeerSize(), 1);
    EXPECT_EQ(same_ip_client.GetConnectedPeerSize(), 0);
    same_ip_client.Stop();
}
