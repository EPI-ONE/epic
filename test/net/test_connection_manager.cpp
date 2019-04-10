#include <gtest/gtest.h>
#include <connection_manager.h>


class TestConnectionManager: public testing::Test {

public:

    ConnectionManager server;
    ConnectionManager client;
    bool test_connect_run = false;
    bool test_connect_inbound;
    void* test_connect_handle = nullptr;
    bool test_disconnect_run = false;
    void* test_disconnect_handle = nullptr;
    std::vector<void*> handle_vector;

    void SetUp() {
        server.Start();
        client.Start();
    }

    void TearDown() {
        server.Stop();
        client.Stop();
    }

    void TestNewConnectionCallback(void *connection_handle, std::string address, bool inbound) {
        std::string direction = inbound? "inbound":"outbound";
        std::cout << "new connection handle:" << connection_handle << " " << address << " " << direction << std::endl;
        test_connect_handle = connection_handle;
        test_connect_run = true;
        test_connect_inbound = inbound;
    }

    void TestDisconnectCallback(void *connection_handle) {
        std::cout << "disconnect handle:" << connection_handle << std::endl;
        test_disconnect_run = true;
        test_disconnect_handle = connection_handle;
    }

    void TestMultiClientNewCallback(void *connection_handle, std::string address, bool inbound) {
        handle_vector.push_back(connection_handle);
    }

};


TEST_F(TestConnectionManager, Listen) {
    server.RegisterNewConnectionCallback(std::bind(&TestConnectionManager::TestNewConnectionCallback,this,std::placeholders::_1,std::placeholders::_2, std::placeholders::_3));

    EXPECT_EQ(server.Listen(7777),0);
    EXPECT_EQ(client.Connect(0x7f000001, 7777),0);

    sleep(1);

    EXPECT_EQ(test_connect_run, true);
    EXPECT_EQ(test_connect_inbound, true);

}

TEST_F(TestConnectionManager, Connect) {
    client.RegisterNewConnectionCallback(std::bind(&TestConnectionManager::TestNewConnectionCallback,this,std::placeholders::_1,std::placeholders::_2, std::placeholders::_3));

    test_connect_inbound = true;

    EXPECT_EQ(server.Listen(7777),0);
    EXPECT_EQ(client.Connect(0x7f000001, 7777),0);

    sleep(1);
    EXPECT_EQ(test_connect_run, true);
    EXPECT_EQ(test_connect_inbound, false);

}

TEST_F(TestConnectionManager, Disconnect) {
    server.RegisterNewConnectionCallback(std::bind(&TestConnectionManager::TestNewConnectionCallback,this,std::placeholders::_1,std::placeholders::_2, std::placeholders::_3));
    client.RegisterDeleteConnectionCallBack(std::bind(&TestConnectionManager::TestDisconnectCallback,this,std::placeholders::_1));

    EXPECT_EQ(server.Listen(7777),0);
    EXPECT_EQ(client.Connect(0x7f000001, 7777),0);

    sleep(1);

    EXPECT_EQ(test_connect_run, true);
    EXPECT_EQ(test_connect_inbound, true);
    server.Disconnect(test_connect_handle);

    sleep(1);
    EXPECT_EQ(test_disconnect_run,true);

}


TEST_F(TestConnectionManager, SendAndReceive) {
    client.RegisterNewConnectionCallback(std::bind(&TestConnectionManager::TestNewConnectionCallback,this,std::placeholders::_1,std::placeholders::_2, std::placeholders::_3));
    client.RegisterDeleteConnectionCallBack(std::bind(&TestConnectionManager::TestDisconnectCallback,this,std::placeholders::_1));

    EXPECT_EQ(server.Listen(7777),0);
    EXPECT_EQ(client.Connect(0x7f000001, 7777),0);

    while (!test_connect_run){};

    int data_len = 4*1000*1000-24;
    uint32_t test_type = 0x55555555;

    std::vector<unsigned char> payload(data_len, 'A');

    NetMessage send_message(test_connect_handle, test_type, payload);
    client.SendMessage(send_message);

    NetMessage receive_message;
    server.ReceiveMessage(receive_message);

    EXPECT_EQ(receive_message.header.magic_number, getMagicNumber());
    EXPECT_EQ(receive_message.header.type,test_type);
    EXPECT_EQ(receive_message.header.payload_length, data_len);
    for (int i=0; i< receive_message.payload.size(); i++) {
        EXPECT_EQ(receive_message.payload.at(i), 'A');
    }

}

TEST_F(TestConnectionManager, SendAndReceiveOnlyHeader) {
    client.RegisterNewConnectionCallback(std::bind(&TestConnectionManager::TestNewConnectionCallback,this,std::placeholders::_1,std::placeholders::_2, std::placeholders::_3));
    client.RegisterDeleteConnectionCallBack(std::bind(&TestConnectionManager::TestDisconnectCallback,this,std::placeholders::_1));

    EXPECT_EQ(server.Listen(7777),0);
    EXPECT_EQ(client.Connect(0x7f000001, 7777),0);

    while (test_connect_run == false){};

    int32_t test_type = 0xAAAAAAAA;

    NetMessage send_message(test_connect_handle, test_type);
    client.SendMessage(send_message);

    NetMessage receive_message;
    server.ReceiveMessage(receive_message);

    EXPECT_EQ(receive_message.header.magic_number, getMagicNumber());
    EXPECT_EQ(receive_message.header.type,test_type);
    EXPECT_EQ(receive_message.header.payload_length, 0);
    EXPECT_EQ(receive_message.payload, send_message.payload);

}

TEST_F(TestConnectionManager, SendAndReceiveMultiMessages) {
    client.RegisterNewConnectionCallback(std::bind(&TestConnectionManager::TestNewConnectionCallback,this,std::placeholders::_1,std::placeholders::_2, std::placeholders::_3));
    client.RegisterDeleteConnectionCallBack(std::bind(&TestConnectionManager::TestDisconnectCallback,this,std::placeholders::_1));

    EXPECT_EQ(server.Listen(7777),0);
    EXPECT_EQ(client.Connect(0x7f000001, 7777),0);

    while (!test_connect_run){};

    int data_len = 2000;
    int num = 100;

    for (int i=0; i<num; i++) {
        std::vector<unsigned char> payload(data_len, 'A'+i);
        NetMessage send_message(test_connect_handle, i, payload);
        client.SendMessage(send_message);
    }

    NetMessage receive_message;
    uint32_t expect_header;

    for (int i=0; i<num; i++) {
        server.ReceiveMessage(receive_message);
        EXPECT_EQ(receive_message.header.magic_number, getMagicNumber());
        EXPECT_EQ(receive_message.header.type, i);
        EXPECT_EQ(receive_message.header.payload_length, data_len);
        for (int j=0; j< receive_message.payload.size(); j++) {
            EXPECT_EQ(receive_message.payload.at(j), 'A'+ i);
        }
    }

}

TEST_F(TestConnectionManager, MultiClient) {

    EXPECT_EQ(server.Listen(7777),0);

    int client_num = 3;
    ConnectionManager client[client_num];

    for (int i = 0; i < client_num; i++) {
        client[i].RegisterNewConnectionCallback(std::bind(&TestConnectionManager::TestMultiClientNewCallback,this,std::placeholders::_1,std::placeholders::_2, std::placeholders::_3));
        client[i].Start();
        EXPECT_EQ(client[i].Connect(0x7f000001, 7777),0);
    }

    sleep(1);

    int data_len = 2000;
    uint32_t message_type = 100;

    for (int i = 0; i< client_num;i++) {
        std::vector<unsigned char> payload(data_len, 'A' + i);
        NetMessage send_message(handle_vector.at(i), message_type, payload);
        client[i].SendMessage(send_message);
        sleep(1);
    }

    for (int i = 0; i< client_num;i++) {
        NetMessage receive_message;
        server.ReceiveMessage(receive_message);
        EXPECT_EQ(receive_message.header.magic_number, getMagicNumber());
        EXPECT_EQ(receive_message.header.type, message_type);
        EXPECT_EQ(receive_message.header.payload_length, data_len);
        for (int j=0; j< receive_message.payload.size(); j++) {
            EXPECT_EQ(receive_message.payload.at(j), 'A' + i);
        }
    }

    for (int i = 0; i< client_num; i++) {
        client[i].Stop();
    }

}
