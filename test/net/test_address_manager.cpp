// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <gtest/gtest.h>

#include "address_manager.h"

class TestAddressManager : public testing::Test {
public:
    AddressManager addressManager;
    std::string addressFilePath = "test_address.toml";

    NetAddress ip1 = *NetAddress::GetByIP("127.0.0.1:7877");
    NetAddress ip2 = *NetAddress::GetByIP("192.168.0.1:7877");
    NetAddress ip3 = *NetAddress::GetByIP("100.0.0.4:7877");
    NetAddress ip4 = *NetAddress::GetByIP("172.4.2.111:7877");

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

TEST_F(TestAddressManager, DeleteInactiveAddr) {
    CONFIG = std::make_unique<Config>();
    addressManager.SaveAddress("./", addressFilePath);
    for (int i = 0; i < CONFIG->GetMaxFailedAttempts() + 1; i++) {
        addressManager.SetLastTry(ip4, time(nullptr));
    }

    addressManager.SaveAddress("./", addressFilePath);
    addressManager.Clear();

    addressManager.LoadAddress("./", addressFilePath);
    std::remove(addressFilePath.c_str());
    EXPECT_FALSE(addressManager.ContainAddress(ip4));
}
