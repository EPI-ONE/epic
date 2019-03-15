#ifndef EPIC_TEST_NET_ADDRESS_HPP
#define EPIC_TEST_NET_ADDRESS_HPP

#include <iostream>
#include <gtest/gtest.h>
#include <net_address.h>

class net_address : public testing::Test {
    protected:
        static void SetUpTestCase() {
            std::cout<<"net address test begins"<<std::endl;
        }

        static void TearDownTestCase() {
            std::cout<<"net address test ends"<<std::endl;
        }

};

TEST_F(net_address, IPAddress_To_String) {
    std::string ip_str = "192.168.0.1";
    IPAddress ip(ip_str);
    EXPECT_STREQ(ip_str.c_str(), ip.ToString().c_str());
}

TEST_F(net_address, NetAddress_To_String) {
    std::string addr_str = "192.168.0.1:7877";
    NetAddress netAddress(addr_str);

    std::string expect_str = '[' + addr_str + ']';
    EXPECT_STREQ(expect_str.c_str(), netAddress.ToString().c_str());
}

#endif //EPIC_TEST_NET_ADDRESS_HPP
