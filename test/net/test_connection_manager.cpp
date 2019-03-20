#include <gtest/gtest.h>
#include <connection_manager.h>


class TestConnectionManager: public testing::Test {
public:
    static void SetUpTestCase() {

    }

    static void TearDownTestCase() {

    }

    void test_accept_cb(evutil_socket_t socket_id, std::string address, bool inbound) {
        std::string direction = inbound? "inbound":"outbound";
        std::cout << "socketid:" << socket_id << " " << address << " " << direction << std::endl;
    }
};


TEST_F(TestConnectionManager, Listen) {
    ConnectionManager server;
    new_connection_callback_t ncb = std::bind(&TestConnectionManager::test_accept_cb,this,std::placeholders::_1,std::placeholders::_2, std::placeholders::_3);
    server.RegisterNewConnectionCallback(ncb);
    int result = server.Listen(7777);
    EXPECT_EQ(result,0);
    server.Start();
    sleep(10);
    server.Stop();
}
