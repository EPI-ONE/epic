#include <gtest/gtest.h>
#include <connection_manager.h>


class TestConnectionManager: public testing::Test {
public:
    static void SetUpTestCase() {

    }

    static void TearDownTestCase() {

    }

    void test_accept_cb(evutil_socket_t socket_id, std::string address) {
        std::cout << "socketid:" << socket_id << " " << address << std::endl;
    }
};


TEST_F(TestConnectionManager, Listen) {
    ConnectionManager cm;
    new_connection_callback_t ncb = std::bind(&TestConnectionManager::test_accept_cb,this,std::placeholders::_1,std::placeholders::_2);
    int result = cm.Listen(7777, ncb);
    EXPECT_EQ(result,0);
}
