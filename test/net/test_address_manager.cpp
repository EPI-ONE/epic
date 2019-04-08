#include <gtest/gtest.h>

#include "address_manager.h"

class TestAddressManager : public testing::Test {
public:
    AddressManager addressManager;
    std::string addressFilePath = "test_address.toml";

    IPAddress ip1 = *IPAddress::StringToIP("127.0.0.1");
    IPAddress ip2 = *IPAddress::StringToIP("192.168.0.1");
    IPAddress ip3 = *IPAddress::StringToIP("100.0.0.4");
    IPAddress ip4 = *IPAddress::StringToIP("172.4.2.111");

    void SetUp() {
        addressManager.AddNewAddress(ip1);
        addressManager.AddNewAddress(ip2);
        addressManager.AddNewAddress(ip3);
        addressManager.AddNewAddress(ip4);
    }

    void TearDown() {
        addressManager.Clear();
    }
};

TEST_F(TestAddressManager, BasicOp) {
    EXPECT_TRUE(addressManager.IsNew(ip1));
    EXPECT_TRUE(addressManager.IsNew(ip2));
    EXPECT_TRUE(addressManager.IsNew(ip3));
    EXPECT_TRUE(addressManager.IsNew(ip4));

    addressManager.MarkOld(ip1);
    addressManager.MarkOld(ip2);

    EXPECT_TRUE(addressManager.IsOld(ip1));
    EXPECT_TRUE(addressManager.IsOld(ip2));
    EXPECT_FALSE(addressManager.IsNew(ip1));
    EXPECT_FALSE(addressManager.IsNew(ip2));

    EXPECT_TRUE(addressManager.ContainAddress(ip1));
    EXPECT_TRUE(addressManager.ContainAddress(ip2));
    EXPECT_TRUE(addressManager.ContainAddress(ip3));
    EXPECT_TRUE(addressManager.ContainAddress(ip4));
}

TEST_F(TestAddressManager, GetAddr) {
    auto addresses = addressManager.GetAddresses();
    EXPECT_EQ(addresses.size(), 4);

    addressManager.MarkOld(ip1);
    addressManager.MarkOld(ip2);

    auto res = addressManager.GetOneAddress(true);
    EXPECT_TRUE(addressManager.IsNew(*res));
}

TEST_F(TestAddressManager, SaveAndLoad) {
    addressManager.SaveAddress("./", addressFilePath);
    addressManager.Clear();
    addressManager.LoadAddress("./", addressFilePath);
    std::remove(addressFilePath.c_str());
    EXPECT_EQ(addressManager.SizeOfAllAddr(), 4);
}

TEST_F(TestAddressManager, LocalAddr) {
    addressManager.LoadLocalAddresses();
    EXPECT_TRUE(addressManager.IsLocal(ip1));
}