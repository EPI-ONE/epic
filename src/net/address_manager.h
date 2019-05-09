#ifndef EPIC_ADDRESS_MANAGER_H
#define EPIC_ADDRESS_MANAGER_H
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

#include "address_message.h"
#include "config.h"
#include "cpptoml.h"
#include "file_utils.h"
#include "net_address.h"
#include "spdlog/spdlog.h"
#include "stream.h"

class NetAddressInfo {
public:
    NetAddressInfo(uint16_t port_, uint64_t lastTry_, uint64_t lastSuccess_, uint16_t numAttempts_)
        : port(port_), lastTry(lastTry_), lastSuccess(lastSuccess_), numAttempts(numAttempts_) {}

    NetAddressInfo() = default;

    // port
    uint16_t port = 0;

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
    bool IsSeedAddress(const IPAddress& address);

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
    bool ContainAddress(const IPAddress& address) const;

    /*
     * add address to new addr map
     * @param address
     * @return
     */
    void AddNewAddress(const IPAddress& address);

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
    void MarkOld(const IPAddress& address);

    /*
     * judge if an address is a new address
     * @param address
     * @return
     */
    bool IsNew(const IPAddress& address) const;

    /*
     * judge if an address is an old address
     * @param address
     * @return
     */
    bool IsOld(const IPAddress& address) const;

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
    void SetLastTry(const IPAddress& address, uint64_t time);

    /*
     * update the time of last success in NetAddressInfo
     * @param address
     * @param time
     */
    void SetLastSuccess(const IPAddress& address, uint64_t time);

    /*
     * get addresses of specified size, default 1000
     * @param size
     */
    std::vector<NetAddress> GetAddresses(size_t size = AddressMessage::kMaxAddressSize);

    /*
     * get the best local address by the score
     * @return
     */
    const IPAddress GetBestLocalAddress();

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
    std::optional<IPAddress> GetOneSeed();

    uint64_t GetLastTry(IPAddress& address);

private:
    /*
     * get the reference of the info of an address
     * @param address
     * @return
     */
    NetAddressInfo* GetInfo(const IPAddress& address);

    // store all the addresses that we haven't try to connect
    std::unordered_map<IPAddress, NetAddressInfo> newAddr_;

    // store all the addresses that we have connected
    std::unordered_map<IPAddress, NetAddressInfo> oldAddr_;

    // recursive lock
    mutable std::recursive_mutex lock_;

    // a set to save all seeds
    std::unordered_set<IPAddress> allSeeds_;

    // a queue to store all seeds that we will connect to
    std::queue<IPAddress> seedQueue_;

    // map of local addresses, value is the score of the local address
    std::unordered_map<IPAddress, uint32_t> localAddresses_;

    std::default_random_engine gen;
};

#endif // EPIC_ADDRESS_MANAGER_H
