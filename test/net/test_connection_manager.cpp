#include <atomic>
#include <connection_manager.h>
#include <gtest/gtest.h>

#include "message_header.h"
#include "sync_messages.h"

class TestConnectionManager : public testing::Test {
public:
    ConnectionManager server;
    ConnectionManager client;
    std::atomic_bool test_connect_run = false;
    std::atomic_bool test_connect_inbound;
    shared_connection_t test_connect_handle = nullptr;
    std::atomic_bool test_disconnect_run    = false;
    std::vector<shared_connection_t> handle_vector;
    std::string test_address;

    void SetUp() {
        server.Start();
        client.Start();
    }

    void TearDown() {
        server.Stop();
        client.Stop();
    }

    void TestNewConnectionCallback(shared_connection_t handle) {
        std::string direction = handle->IsInbound() ? "inbound" : "outbound";
        test_connect_handle   = handle;
        test_connect_run      = true;
        test_connect_inbound  = handle->IsInbound();
        test_address          = handle->GetRemote();
        std::cout << "new connection handle:" << handle << " " << test_address << " " << direction << std::endl;
    }

    void TestDisconnectCallback(shared_connection_t handle) {
        std::cout << "disconnect handle:" << handle << std::endl;
        test_disconnect_run = true;
        test_connect_handle.reset();
    }

    void TestMultiClientNewCallback(shared_connection_t handle) {
        handle_vector.push_back(handle);
    }
};

TEST_F(TestConnectionManager, Listen) {
    server.RegisterNewConnectionCallback(
        std::bind(&TestConnectionManager::TestNewConnectionCallback, this, std::placeholders::_1));

    ASSERT_TRUE(server.Bind(0x7f000001));
    ASSERT_TRUE(server.Listen(12345));
    ASSERT_TRUE(client.Connect(0x7f000001, 12345));

    usleep(50000);

    EXPECT_EQ(test_connect_run, true);
    EXPECT_EQ(test_connect_inbound, true);
    EXPECT_EQ(server.GetInboundNum(), 1);
    EXPECT_EQ(server.GetOutboundNum(), 0);
    EXPECT_EQ(server.GetConnectionNum(), 1);
    test_connect_handle->Disconnect();
}

TEST_F(TestConnectionManager, Listen_fail) {
    ASSERT_TRUE(server.Listen(43210));
    EXPECT_FALSE(client.Listen(43210));
}

TEST_F(TestConnectionManager, Connect) {
    client.RegisterNewConnectionCallback(
        std::bind(&TestConnectionManager::TestNewConnectionCallback, this, std::placeholders::_1));

    test_connect_inbound = true;

    ASSERT_TRUE(server.Bind(0x7f000001));
    ASSERT_TRUE(server.Listen(7890));
    ASSERT_TRUE(client.Connect(0x7f000001, 7890));

    usleep(50000);
    EXPECT_EQ(test_connect_run, true);
    EXPECT_EQ(test_connect_inbound, false);
    EXPECT_EQ(test_address, "127.0.0.1:7890");
    EXPECT_EQ(client.GetInboundNum(), 0);
    EXPECT_EQ(client.GetOutboundNum(), 1);
    EXPECT_EQ(client.GetConnectionNum(), 1);
    EXPECT_EQ(server.GetInboundNum(), 1);
    EXPECT_EQ(server.GetOutboundNum(), 0);
    EXPECT_EQ(server.GetConnectionNum(), 1);
    test_connect_handle->Disconnect();
}

TEST_F(TestConnectionManager, Disconnect) {
    server.RegisterNewConnectionCallback(
        std::bind(&TestConnectionManager::TestNewConnectionCallback, this, std::placeholders::_1));
    client.RegisterDeleteConnectionCallBack(
        std::bind(&TestConnectionManager::TestDisconnectCallback, this, std::placeholders::_1));

    ASSERT_TRUE(server.Bind(0x7f000001));
    ASSERT_TRUE(server.Listen(27877));
    ASSERT_TRUE(client.Connect(0x7f000001, 27877));

    usleep(50000);
    EXPECT_EQ(test_connect_run, true);
    EXPECT_EQ(test_connect_inbound, true);
    EXPECT_EQ(server.GetInboundNum(), 1);
    EXPECT_EQ(server.GetOutboundNum(), 0);
    EXPECT_EQ(server.GetConnectionNum(), 1);
    EXPECT_EQ(client.GetInboundNum(), 0);
    EXPECT_EQ(client.GetOutboundNum(), 1);
    EXPECT_EQ(client.GetConnectionNum(), 1);
    test_connect_handle->Disconnect();

    usleep(50000);
    EXPECT_EQ(test_disconnect_run, true);
    EXPECT_EQ(server.GetInboundNum(), 0);
    EXPECT_EQ(server.GetOutboundNum(), 0);
    EXPECT_EQ(server.GetConnectionNum(), 0);
    EXPECT_EQ(client.GetInboundNum(), 0);
    EXPECT_EQ(client.GetOutboundNum(), 0);
    EXPECT_EQ(client.GetConnectionNum(), 0);
}

TEST_F(TestConnectionManager, SendAndReceive) {
    client.RegisterNewConnectionCallback(
        std::bind(&TestConnectionManager::TestNewConnectionCallback, this, std::placeholders::_1));
    client.RegisterDeleteConnectionCallBack(
        std::bind(&TestConnectionManager::TestDisconnectCallback, this, std::placeholders::_1));

    ASSERT_TRUE(server.Bind(0x7f000001));
    ASSERT_TRUE(server.Listen(51001));
    ASSERT_TRUE(client.Connect(0x7f000001, 51001));

    usleep(50000);

    size_t size    = MAX_MESSAGE_LENGTH / 32 - 32;
    uint32_t nonce = 0x55555555;
    uint256 h      = uintS<256>(std::string(64, 'a'));
    std::vector<uint256> data(size, h);
    test_connect_handle->SendMessage(std::make_unique<Inv>(data, nonce));

    usleep(50000);
    connection_message_t receive_message;
    ASSERT_TRUE(server.ReceiveMessage(receive_message));

    Inv* msg = dynamic_cast<Inv*>(receive_message.second.get());

    ASSERT_TRUE(msg != nullptr);
    ASSERT_EQ(msg->GetType(), NetMessage::INV);
    ASSERT_EQ(msg->nonce, nonce);
    ASSERT_EQ(msg->hashes.size(), size);
    for (auto hash : msg->hashes) {
        ASSERT_EQ(h, hash);
    }

    test_connect_handle->Disconnect();
}

TEST_F(TestConnectionManager, SendAndReceiveOnlyHeader) {
    client.RegisterNewConnectionCallback(
        std::bind(&TestConnectionManager::TestNewConnectionCallback, this, std::placeholders::_1));
    client.RegisterDeleteConnectionCallBack(
        std::bind(&TestConnectionManager::TestDisconnectCallback, this, std::placeholders::_1));

    ASSERT_TRUE(server.Bind(0x7f000001));
    ASSERT_TRUE(server.Listen(51010));
    ASSERT_TRUE(client.Connect(0x7f000001, 51010));

    usleep(50000);

    test_connect_handle->SendMessage(std::make_unique<NetMessage>(NetMessage::VERSION_ACK));

    usleep(50000);

    connection_message_t receive_message;
    ASSERT_TRUE(server.ReceiveMessage(receive_message));

    ASSERT_EQ(receive_message.second->GetType(), NetMessage::VERSION_ACK);
    test_connect_handle->Disconnect();
}

TEST_F(TestConnectionManager, SendAndReceiveMultiMessages) {
    client.RegisterNewConnectionCallback(
        std::bind(&TestConnectionManager::TestNewConnectionCallback, this, std::placeholders::_1));
    client.RegisterDeleteConnectionCallBack(
        std::bind(&TestConnectionManager::TestDisconnectCallback, this, std::placeholders::_1));

    ASSERT_TRUE(server.Bind(0x7f000001));
    ASSERT_TRUE(server.Listen(51020));
    ASSERT_TRUE(client.Connect(0x7f000001, 51020));

    usleep(50000);

    int num     = 3;
    size_t size = 1000;

    for (int i = 0; i < num; i++) {
        uint32_t nonce = 0x55555555 + i;
        uint256 h      = uintS<256>(std::string(64, 'a' + i));
        std::vector<uint256> data(size, h);

        test_connect_handle->SendMessage(std::make_unique<Inv>(data, nonce));
        usleep(50000);
    }

    connection_message_t receive_message;

    for (int i = 0; i < num; i++) {
        uint32_t nonce = 0x55555555 + i;
        uint256 h      = uintS<256>(std::string(64, 'a' + i));
        ASSERT_TRUE(server.ReceiveMessage(receive_message));
        Inv* msg = dynamic_cast<Inv*>(receive_message.second.get());
        ASSERT_TRUE(msg != nullptr);
        ASSERT_EQ(msg->GetType(), NetMessage::INV);
        ASSERT_EQ(msg->nonce, nonce);
        ASSERT_EQ(msg->hashes.size(), size);
        for (auto hash : msg->hashes) {
            ASSERT_EQ(h, hash);
        }
    }
    test_connect_handle->Disconnect();
}

TEST_F(TestConnectionManager, MultiClient) {
    ASSERT_TRUE(server.Bind(0x7f000001));
    ASSERT_TRUE(server.Listen(51030));

    int client_num = 3;
    ConnectionManager client[client_num];

    for (int i = 0; i < client_num; i++) {
        client[i].RegisterNewConnectionCallback(
            std::bind(&TestConnectionManager::TestMultiClientNewCallback, this, std::placeholders::_1));
        client[i].Start();
        ASSERT_TRUE(client[i].Connect(0x7f000001, 51030));
    }

    usleep(50000);
    EXPECT_EQ(server.GetInboundNum(), 3);
    EXPECT_EQ(server.GetOutboundNum(), 0);
    EXPECT_EQ(server.GetConnectionNum(), 3);

    size_t size = 1000;

    for (int i = 0; i < client_num; i++) {
        uint32_t nonce = 0x55555555 + i;
        uint256 h      = uintS<256>(std::string(64, 'a' + i));
        std::vector<uint256> data(size, h);
        handle_vector.at(i)->SendMessage(std::make_unique<Inv>(data, nonce));
        usleep(50000);
    }

    for (int i = 0; i < client_num; i++) {
        uint32_t nonce = 0x55555555 + i;
        uint256 h      = uintS<256>(std::string(64, 'a' + i));
        connection_message_t receive_message;
        ASSERT_TRUE(server.ReceiveMessage(receive_message));
        Inv* msg = dynamic_cast<Inv*>(receive_message.second.get());
        ASSERT_TRUE(msg != nullptr);
        ASSERT_EQ(msg->GetType(), NetMessage::INV);
        ASSERT_EQ(msg->nonce, nonce);
        ASSERT_EQ(msg->hashes.size(), size);
        for (auto hash : msg->hashes) {
            ASSERT_EQ(h, hash);
        }
    }

    for (int i = 0; i < client_num; i++) {
        handle_vector.at(i)->Disconnect();
        client[i].Stop();
    }
    handle_vector.clear();

    usleep(100000);
    EXPECT_EQ(server.GetInboundNum(), 0);
    EXPECT_EQ(server.GetOutboundNum(), 0);
    EXPECT_EQ(server.GetConnectionNum(), 0);
}

TEST_F(TestConnectionManager, Bind_fail) {
    ASSERT_FALSE(server.Bind(0x5A5A5A5A));
}
