// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef EPIC_ADDRESS_MANAGER_H
#define EPIC_ADDRESS_MANAGER_H

#include "address_message.h"
#include "config.h"
#include "cpptoml.h"
#include "file_utils.h"
#include "net_address.h"
#include "spdlog/spdlog.h"
#include "stream.h"

#include <arpa/inet.h>
#include <fstream>
#include <ifaddrs.h>
#include <list>
#include <mutex>
#include <optional>
#include <queue>
#include <random>
#include <time.h>
#include <unordered_map>
#include <unordered_set>

class NetAddressInfo {
public:
    NetAddressInfo(uint64_t lastTry_, uint64_t lastSuccess_, uint16_t numAttempts_)
        : lastTry(lastTry_), lastSuccess(lastSuccess_), numAttempts(numAttempts_) {}

    NetAddressInfo() = default;

    // last time that we tried to connect to the address
    uint64_t lastTry = 0;

    // last time we succeeded in connecting to the address
    uint64_t lastSuccess = 0;

    // connection attempts since last successful attempt
    uint16_t numAttempts = 0;
};

class AddressManager {
public:
    AddressManager();

    /*
     * load addresses, load seeds
     */
    void Init();

    /*
     * judge if a net address is a seed
     * @param address
     * @return bool
     */
    bool IsSeedAddress(const NetAddress& address);

    /*
     * dump all addresses as toml file
     */
    void SaveAddress(const std::string& path, const std::string& filename);

    /*
     * load all addresses from the file
     */
    void LoadAddress(const std::string& path, const std::string& filename);

    /*
     * if the address is contained in the maps
     * @param address
     * @return
     */
    bool ContainAddress(const NetAddress& address) const;

    /*
     * add address to new addr map
     * @param address
     */
    void AddNewAddress(const NetAddress& address);

    /*
     * 1. update the information of an address
     * 2. put the address from new map to old map if it is new
     * @param address
     */
    void MarkOld(const NetAddress& address);

    /*
     * judge if an address is a new address
     * @param address
     * @return
     */
    bool IsNew(const NetAddress& address) const;

    /*
     * judge if an address is an old address
     * @param address
     * @return
     */
    bool IsOld(const NetAddress& address) const;

    /*
     * get an address to connect from all address map
     * @param onlyNew
     * @return
     */
    std::optional<NetAddress> GetOneAddress(bool onlyNew);

    /*
     * update the time of last try in NetAddressInfo
     * @param address
     * @param time
     */
    void SetLastTry(const NetAddress& address, uint64_t time);

    /*
     * update the time of last success in NetAddressInfo
     * @param address
     * @param time
     */
    void SetLastSuccess(const NetAddress& address, uint64_t time);

    /*
     * get addresses of specified size, default 1000
     * @param size
     */
    std::vector<NetAddress> GetAddresses(size_t size = AddressMessage::kMaxAddressSize);

    /*
     * get the best local address by the score
     * @return
     */
    const NetAddress GetBestLocalAddress();

    /*
     * add the score of the local address
     * @param ipAddress
     */
    void SeenLocalAddress(IPAddress& ipAddress);

    /*
     * judge if an address is a local address
     * @param address
     * @return
     */
    bool IsLocal(const IPAddress& address) const;

    /*
     * clear all saved address
     */
    void Clear();

    /*
     * get size of all saved addresses
     * @return
     */
    size_t SizeOfAllAddr();

    /*
     * load all local addresses(ipv4 and ipv6) from network interfaces
     */
    void LoadLocalAddresses();

    /**
     * get one seed that hasn't been connected
     * @return
     */
    std::optional<NetAddress> GetOneSeed();

    uint64_t GetLastTry(NetAddress& address);

private:
    /*
     * get the reference of the info of an address
     * @param address
     * @return
     */
    NetAddressInfo* GetInfo(const NetAddress& address);

    // store all the addresses that we haven't try to connect
    std::unordered_map<NetAddress, NetAddressInfo> newAddr_;

    // store all the addresses that we have connected
    std::unordered_map<NetAddress, NetAddressInfo> oldAddr_;

    // recursive lock
    mutable std::recursive_mutex lock_;

    // a set to save all seeds
    std::unordered_set<NetAddress> allSeeds_;

    // a queue to store all seeds that we will connect to
    std::queue<NetAddress> seedQueue_;

    // map of local addresses, value is the score of the local address
    std::unordered_map<IPAddress, uint32_t> localAddresses_;

    std::default_random_engine gen;
};

#endif // EPIC_ADDRESS_MANAGER_H
