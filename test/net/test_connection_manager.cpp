#include <atomic>
#include <connection_manager.h>
#include <gtest/gtest.h>

class TestConnectionManager : public testing::Test {
public:
    ConnectionManager server;
    ConnectionManager client;
    std::atomic_bool test_connect_run = false;
    std::atomic_bool test_connect_inbound;
    std::atomic<void*> test_connect_handle = nullptr;
    std::atomic_bool test_disconnect_run   = false;
    std::vector<void*> handle_vector;
    std::string test_address;

    void SetUp() {
        server.Start();
        client.Start();
    }

    void TearDown() {
        server.Stop();
        client.Stop();
    }

    void TestNewConnectionCallback(void* connection_handle, std::string& address, bool inbound) {
        std::string direction = inbound ? "inbound" : "outbound";
        std::cout << "new connection handle:" << connection_handle << " " << address << " " << direction << std::endl;
        test_connect_handle  = connection_handle;
        test_connect_run     = true;
        test_connect_inbound = inbound;
        test_address         = address;
    }

    void TestDisconnectCallback(void* connection_handle) {
        std::cout << "disconnect handle:" << connection_handle << std::endl;
        test_disconnect_run = true;
    }

    void TestMultiClientNewCallback(void* connection_handle, std::string&, bool) {
        handle_vector.push_back(connection_handle);
    }
};

TEST_F(TestConnectionManager, Listen) {
    server.RegisterNewConnectionCallback(std::bind(&TestConnectionManager::TestNewConnectionCallback, this,
        std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

    ASSERT_TRUE(server.Bind(0x7f000001));
    ASSERT_TRUE(server.Listen(12345));
    ASSERT_TRUE(client.Connect(0x7f000001, 12345));

    usleep(50000);

    EXPECT_EQ(test_connect_run, true);
    EXPECT_EQ(test_connect_inbound, true);
    EXPECT_EQ(server.GetInboundNum(), 1);
    EXPECT_EQ(server.GetOutboundNum(), 0);
    EXPECT_EQ(server.GetConnectionNum(), 1);
}

TEST_F(TestConnectionManager, Listen_fail) {
    ASSERT_TRUE(server.Listen(43210));
    EXPECT_FALSE(client.Listen(43210));
}

TEST_F(TestConnectionManager, Connect) {
    client.RegisterNewConnectionCallback(std::bind(&TestConnectionManager::TestNewConnectionCallback, this,
        std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

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
}

TEST_F(TestConnectionManager, Disconnect) {
    server.RegisterNewConnectionCallback(std::bind(&TestConnectionManager::TestNewConnectionCallback, this,
        std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    client.RegisterDeleteConnectionCallBack(
        std::bind(&TestConnectionManager::TestDisconnectCallback, this, std::placeholders::_1));

    ASSERT_TRUE(server.Bind(0x7f000001));
    ASSERT_TRUE(server.Listen(51234));
    ASSERT_TRUE(client.Connect(0x7f000001, 51234));

    usleep(50000);
    EXPECT_EQ(test_connect_run, true);
    EXPECT_EQ(test_connect_inbound, true);
    EXPECT_EQ(server.GetInboundNum(), 1);
    EXPECT_EQ(server.GetOutboundNum(), 0);
    EXPECT_EQ(server.GetConnectionNum(), 1);
    server.Disconnect(test_connect_handle);

    usleep(50000);
    EXPECT_EQ(test_disconnect_run, true);
    EXPECT_EQ(server.GetInboundNum(), 0);
    EXPECT_EQ(server.GetOutboundNum(), 0);
    EXPECT_EQ(server.GetConnectionNum(), 0);
}

TEST_F(TestConnectionManager, SendAndReceive) {
    client.RegisterNewConnectionCallback(std::bind(&TestConnectionManager::TestNewConnectionCallback, this,
        std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    client.RegisterDeleteConnectionCallBack(
        std::bind(&TestConnectionManager::TestDisconnectCallback, this, std::placeholders::_1));

    ASSERT_TRUE(server.Bind(0x7f000001));
    ASSERT_TRUE(server.Listen(51001));
    ASSERT_TRUE(client.Connect(0x7f000001, 51001));

    usleep(50000);

    int data_len       = 4 * 1000 * 1000 - 24;
    uint32_t test_type = 0x55555555;

    VStream payload;
    payload.resize(data_len);
    memset(payload.data(), 'A', data_len);

    NetMessage send_message(test_connect_handle, test_type, payload);
    client.SendMessage(send_message);

    NetMessage receive_message;
    server.ReceiveMessage(receive_message);

    EXPECT_EQ(receive_message.header.magic_number, GetMagicNumber());
    EXPECT_EQ(receive_message.header.type, test_type);
    EXPECT_EQ(receive_message.header.payload_length, data_len);

    for (size_t i = 0; i < receive_message.payload.size(); i++) {
        EXPECT_EQ(receive_message.payload[i], 'A');
    }
}

TEST_F(TestConnectionManager, SendAndReceiveOnlyHeader) {
    client.RegisterNewConnectionCallback(std::bind(&TestConnectionManager::TestNewConnectionCallback, this,
        std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    client.RegisterDeleteConnectionCallBack(
        std::bind(&TestConnectionManager::TestDisconnectCallback, this, std::placeholders::_1));

    ASSERT_TRUE(server.Bind(0x7f000001));
    ASSERT_TRUE(server.Listen(51010));
    ASSERT_TRUE(client.Connect(0x7f000001, 51010));

    usleep(50000);

    uint32_t test_type = 0xAAAAAAAA;

    NetMessage send_message(test_connect_handle, test_type);
    client.SendMessage(send_message);

    NetMessage receive_message;
    server.ReceiveMessage(receive_message);

    EXPECT_EQ(receive_message.header.magic_number, GetMagicNumber());
    EXPECT_EQ(receive_message.header.type, test_type);
    EXPECT_EQ(receive_message.header.payload_length, 0);
    EXPECT_EQ(receive_message.payload, send_message.payload);
}

TEST_F(TestConnectionManager, SendAndReceiveMultiMessages) {
    client.RegisterNewConnectionCallback(std::bind(&TestConnectionManager::TestNewConnectionCallback, this,
        std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    client.RegisterDeleteConnectionCallBack(
        std::bind(&TestConnectionManager::TestDisconnectCallback, this, std::placeholders::_1));

    ASSERT_TRUE(server.Bind(0x7f000001));
    ASSERT_TRUE(server.Listen(51020));
    ASSERT_TRUE(client.Connect(0x7f000001, 51020));

    usleep(50000);

    int data_len = 2000;
    int num      = 3;

    for (int i = 0; i < num; i++) {
        VStream payload;
        payload.resize(data_len);
        memset(payload.data(), 'A' + i, data_len);

        NetMessage send_message(test_connect_handle, i, payload);
        client.SendMessage(send_message);
    }

    NetMessage receive_message;

    for (int i = 0; i < num; i++) {
        server.ReceiveMessage(receive_message);
        EXPECT_EQ(receive_message.header.magic_number, GetMagicNumber());
        EXPECT_EQ(receive_message.header.type, i);
        EXPECT_EQ(receive_message.header.payload_length, data_len);
        for (size_t j = 0; j < receive_message.payload.size(); j++) {
            EXPECT_EQ(receive_message.payload[j], 'A' + i);
        }
    }
}

TEST_F(TestConnectionManager, MultiClient) {
    ASSERT_TRUE(server.Bind(0x7f000001));
    ASSERT_TRUE(server.Listen(51030));

    int client_num = 3;
    ConnectionManager client[client_num];

    for (int i = 0; i < client_num; i++) {
        client[i].RegisterNewConnectionCallback(std::bind(&TestConnectionManager::TestMultiClientNewCallback, this,
            std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
        client[i].Start();
        ASSERT_TRUE(client[i].Connect(0x7f000001, 51030));
    }

    usleep(50000);
    EXPECT_EQ(server.GetInboundNum(), 3);
    EXPECT_EQ(server.GetOutboundNum(), 0);
    EXPECT_EQ(server.GetConnectionNum(), 3);

    int data_len          = 2000;
    uint32_t message_type = 100;

    for (int i = 0; i < client_num; i++) {
        VStream payload;
        payload.resize(data_len);
        memset(payload.data(), 'A' + i, data_len);

        NetMessage send_message(handle_vector.at(i), message_type, payload);
        client[i].SendMessage(send_message);
        usleep(50000);
    }

    for (int i = 0; i < client_num; i++) {
        NetMessage receive_message;
        server.ReceiveMessage(receive_message);
        EXPECT_EQ(receive_message.header.magic_number, GetMagicNumber());
        EXPECT_EQ(receive_message.header.type, message_type);
        EXPECT_EQ(receive_message.header.payload_length, data_len);
        for (size_t j = 0; j < receive_message.payload.size(); j++) {
            EXPECT_EQ(receive_message.payload[j], 'A' + i);
        }
    }

    for (int i = 0; i < client_num; i++) {
        client[i].Stop();
    }

    usleep(100000);
    EXPECT_EQ(server.GetInboundNum(), 0);
    EXPECT_EQ(server.GetOutboundNum(), 0);
    EXPECT_EQ(server.GetConnectionNum(), 0);
}

TEST_F(TestConnectionManager, Bind_fail) {
    ASSERT_FALSE(server.Bind(0x5A5A5A5A));
}
