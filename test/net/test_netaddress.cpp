#include <gtest/gtest.h>
#include <net_address.h>

class TestNetAddress : public testing::Test {
public:
    std::optional<NetAddress> netAddress;
    std::optional<IPAddress> ipAddress;
};

TEST_F(TestNetAddress, IPAddress) {
    // some correct ip string
    std::string ip4_1 = "127.0.0.4";
    std::string ip4_2 = "3.6.8.111";
    std::string ip6_1 = "1::8";
    std::string ip6_2 = "2001:2db8:5210:1230:7877:ff00:8942:8329";

    EXPECT_TRUE(ipAddress = IPAddress::GetByIP(ip4_1));
    EXPECT_EQ(ipAddress->ToString(), ip4_1);
    EXPECT_TRUE(ipAddress = IPAddress::GetByIP(ip4_2));
    EXPECT_EQ(ipAddress->ToString(), ip4_2);
    EXPECT_TRUE(ipAddress = IPAddress::GetByIP(ip6_1));
    // since the ToString() doesn't support the abbreviation
    EXPECT_EQ(ipAddress->ToString(), "1:0:0:0:0:0:0:8");
    EXPECT_TRUE(ipAddress = IPAddress::GetByIP(ip6_2));
    EXPECT_EQ(ipAddress->ToString(), ip6_2);


    // some wrong ip string
    std::string iperr_1 = "256.1.1.1";
    std::string iperr_2 = "2.a.3.d";
    std::string iperr_3 = "1.1.1..";
    std::string iperr_4 = "5:2:2.2";
    std::string iperr_5 = "asdlkfj::we::::0";
    std::string iperr_6 = "123::6::::2::::9999";

    EXPECT_FALSE(IPAddress::GetByIP(iperr_1));
    EXPECT_FALSE(IPAddress::GetByIP(iperr_2));
    EXPECT_FALSE(IPAddress::GetByIP(iperr_3));
    EXPECT_FALSE(IPAddress::GetByIP(iperr_4));
    EXPECT_FALSE(IPAddress::GetByIP(iperr_5));
    EXPECT_FALSE(IPAddress::GetByIP(iperr_6));
}

TEST_F(TestNetAddress, NetAddress) {
    // some correct ip + port
    std::string addr_1 = "127.0.0.4:7877";
    std::string addr_2 = "[2001:2db8:5210:1230:7877:ff00:8942:8329]:1234";

    EXPECT_TRUE(netAddress = NetAddress::GetByIP(addr_1));
    EXPECT_EQ(addr_1, netAddress->ToString());
    EXPECT_TRUE(netAddress = NetAddress::GetByIP(addr_2));
    EXPECT_EQ(addr_2, netAddress->ToString());

    // some wrong ip + port
    std::string err_1 = "127.0.0.4:12:45";
    std::string err_2 = "127.0.0.4::45";
    std::string err_3 = "127.0.0.4:1111111";
    std::string err_4 = "[123:21ed:123::::1001]:7871:1";
    std::string err_5 = "2001:2db8:5210:1230:7877:ff00:8942:8329:1234";

    EXPECT_FALSE(NetAddress::GetByIP(err_1));
    EXPECT_FALSE(NetAddress::GetByIP(err_2));
    EXPECT_FALSE(NetAddress::GetByIP(err_3));
    EXPECT_FALSE(NetAddress::GetByIP(err_4));
    EXPECT_FALSE(NetAddress::GetByIP(err_5));
}
