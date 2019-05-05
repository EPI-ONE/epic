#include "address_manager.h"

AddressManager::AddressManager() {
    std::random_device rd;
    gen = std::default_random_engine(rd());
}

void AddressManager::Init() {
    for (const NetAddress& seed : config->getSeeds()) {
        allSeeds_.insert(seed);
    }

    LoadLocalAddresses();
    LoadAddress(config->getAddressPath(), config->getAddressFilename());
}

bool AddressManager::IsSeedAddress(const IPAddress& address) {
    return allSeeds_.find(address) != allSeeds_.end();
}

bool AddressManager::ContainAddress(const IPAddress& address) const {
    std::lock_guard<std::recursive_mutex> lk(lock_);
    return newAddr_.find(address) != newAddr_.end() || oldAddr_.find(address) != oldAddr_.end();
}

void AddressManager::AddNewAddress(const IPAddress& address) {
    if (IsSeedAddress(address) || IsLocal(address)) {
        return;
    }

    if (ContainAddress(address)) {
        return;
    }
    newAddr_.insert(std::make_pair(address, NetAddressInfo()));
}

void AddressManager::AddNewAddress(const NetAddress& address) {
    if (IsSeedAddress(address) || IsLocal(address)) {
        return;
    }
    std::lock_guard<std::recursive_mutex> lk(lock_);
    if (newAddr_.find(address) != newAddr_.end()) {
        return;
    }
    NetAddressInfo info;
    info.port = address.GetPort();
    newAddr_.insert(std::make_pair(address, info));
}

void AddressManager::MarkOld(const IPAddress& address) {
    if (!ContainAddress(address)) {
        return;
    }
    std::lock_guard<std::recursive_mutex> lk(lock_);
    NetAddressInfo* info = GetInfo(address);
    info->lastSuccess    = time(nullptr);
    info->numAttempts    = 0;
    if (IsNew(address)) {
        oldAddr_.insert(std::make_pair(address, *info));
        newAddr_.erase(address);
    }
}

void AddressManager::SetLastTry(const IPAddress& address, uint64_t time) {
    NetAddressInfo* info = GetInfo(address);
    if (!info) {
        return;
    }
    std::lock_guard<std::recursive_mutex> lk(lock_);
    info->numAttempts++;
    info->lastTry = time;
}

void AddressManager::SetLastSuccess(const IPAddress& address, uint64_t time) {
    NetAddressInfo* info = GetInfo(address);
    if (!info) {
        return;
    }
    std::lock_guard<std::recursive_mutex> lk(lock_);
    info->numAttempts = 0;
    info->lastSuccess = time;
}

NetAddressInfo* AddressManager::GetInfo(const IPAddress& address) {
    std::lock_guard<std::recursive_mutex> lk(lock_);
    auto it = newAddr_.find(address);
    if (it != newAddr_.end()) {
        return &(it->second);
    } else {
        it = oldAddr_.find(address);
        if (it != oldAddr_.end()) {
            return &(it->second);
        }
    }
    return nullptr;
}

std::optional<NetAddress> AddressManager::GetOneAddress(bool onlyNew) {
    std::lock_guard<std::recursive_mutex> lk(lock_);
    if (onlyNew && newAddr_.empty()) {
        return {};
    }
    std::uniform_int_distribution<int> dis1(0, 1);
    bool useNew = dis1(gen) == 0;
    if (!onlyNew && !oldAddr_.empty() && !useNew) {
        std::uniform_int_distribution<int> dis2(0, oldAddr_.size() - 1);
        int index = dis2(gen);
        auto it   = oldAddr_.begin();
        std::next(it, index);
        return NetAddress(it->first, it->second.port);
    } else if (!newAddr_.empty()) {
        std::uniform_int_distribution<int> dis3(0, newAddr_.size() - 1);
        int index = dis3(gen);
        auto it   = newAddr_.begin();
        std::next(it, index);
        return NetAddress(it->first, it->second.port);
    }
    return {};
}

void AddressManager::SaveAddress(const std::string& path, const std::string& filename) {
    if (!CheckDirExist(path)) {
        std::cerr << "The address dir \"" << path << "\" not found, try to create the directory..." << std::endl;
        if (Mkdir_recursive(path)) {
            std::cerr << path << " has been created" << std::endl;
        } else {
            throw spdlog::spdlog_ex("fail to create the address path");
        }
    }
    std::ofstream file;
    file.open(path + filename, std::ios::out | std::ios::trunc);
    auto root      = cpptoml::make_table();
    auto new_table = cpptoml::make_table_array();
    // new addresses
    for (auto& it : newAddr_) {
        auto table = cpptoml::make_table();
        table->insert("numAttempts", it.second.numAttempts);
        table->insert("lastSuccess", it.second.lastSuccess);
        table->insert("lastTry", it.second.lastTry);
        table->insert("port", it.second.port);
        table->insert("ip", it.first.ToString());
        new_table->push_back(table);
    }
    root->insert("new", new_table);

    auto old_table = cpptoml::make_table_array();
    for (auto& it : oldAddr_) {
        auto table = cpptoml::make_table();
        table->insert("numAttempts", it.second.numAttempts);
        table->insert("lastSuccess", it.second.lastSuccess);
        table->insert("lastTry", it.second.lastTry);
        table->insert("lastTry", it.second.lastTry);
        table->insert("port", it.second.port);
        table->insert("ip", it.first.ToString());
        old_table->push_back(table);
    }
    root->insert("old", old_table);

    file << *root;
    file.close();
    spdlog::info("save all addresses to {}", path + filename);
}

void AddressManager::LoadAddress(const std::string& path, const std::string& filename) {
    if (!CheckFileExist(path + filename)) {
        spdlog::warn("can not find the address.toml to load");
        return;
    }
    auto all_address = cpptoml::parse_file(path + filename);
    if (all_address) {
        auto new_table = all_address->get_table_array("new");
        if (new_table) {
            for (const auto& table : *new_table) {
                auto ip          = table->get_as<std::string>("ip");
                auto port        = table->get_as<uint16_t>("port");
                auto lastTry     = table->get_as<uint64_t>("lastTry");
                auto lastSuccess = table->get_as<uint64_t>("lastSuccess");
                auto numAttempts = table->get_as<uint>("numAttempts");
                newAddr_.insert(std::make_pair(
                    *IPAddress::StringToIP(*ip), NetAddressInfo(*port, *lastTry, *lastSuccess, *numAttempts)));
            }
        }
        auto old_table = all_address->get_table_array("old");
        if (old_table) {
            for (const auto& table : *old_table) {
                auto ip          = table->get_as<std::string>("ip");
                auto port        = table->get_as<uint16_t>("port");
                auto lastTry     = table->get_as<uint64_t>("lastTry");
                auto lastSuccess = table->get_as<uint64_t>("lastSuccess");
                auto numAttempts = table->get_as<uint>("numAttempts");
                oldAddr_.insert(std::make_pair(
                    *IPAddress::StringToIP(*ip), NetAddressInfo(*port, *lastTry, *lastSuccess, *numAttempts)));
            }
        }
    }
}

std::vector<NetAddress> AddressManager::GetAddresses(size_t size) {
    std::lock_guard<std::recursive_mutex> lk(lock_);
    size_t total_size  = newAddr_.size() + oldAddr_.size();
    size_t actual_size = std::min(size, total_size);
    std::vector<NetAddress> list;
    if (actual_size == 0) {
        return list;
    }
    list.reserve(actual_size);
    for (auto& it : oldAddr_) {
        list.emplace_back(it.first, it.second.port);
        if (list.size() == actual_size) {
            return list;
        }
    }
    for (auto& it : newAddr_) {
        list.emplace_back(it.first, it.second.port);
        if (list.size() == actual_size) {
            return list;
        }
    }
    return list;
}

void AddressManager::LoadLocalAddresses() {
    struct ifaddrs* ifAddrStruct = nullptr;
    struct ifaddrs* ifa          = nullptr;

    getifaddrs(&ifAddrStruct);

    for (ifa = ifAddrStruct; ifa != nullptr; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr) {
            continue;
        }
        uint_fast32_t w;
        uint32_t score = 0;
        IPAddress ip;
        if (ifa->ifa_addr->sa_family == AF_INET) { // check it is IP4
            // is a valid IP4 Address
            ip = IPAddress(((struct sockaddr_in*) ifa->ifa_addr)->sin_addr);
        } else if (ifa->ifa_addr->sa_family == AF_INET6) { // check it is IP6
            // is a valid IP6 Address
            ip = IPAddress(((struct sockaddr_in6*) ifa->ifa_addr)->sin6_addr);
        } else {
            continue;
        }
        if (ip.IsRoutable()) {
            score++;
        }
        localAddresses_.insert(std::make_pair(ip, score));
        //        spdlog::info("find local address: {} --> {}", ifa->ifa_name, ip.ToString());
    }
    if (ifAddrStruct != nullptr)
        freeifaddrs(ifAddrStruct);
}

const IPAddress AddressManager::GetBestLocalAddress() {
    assert(!localAddresses_.empty());
    auto best = localAddresses_.begin();
    for (auto it = localAddresses_.begin(); it != localAddresses_.end(); it++) {
        best = it->second > best->second ? it : best;
    }
    return best->first;
}

void AddressManager::SeenLocalAddress(IPAddress& ipAddress) {
    auto it = localAddresses_.find(ipAddress);
    if (it != localAddresses_.end()) {
        it->second++;
    }
}

bool AddressManager::IsLocal(const IPAddress& address) const {
    return localAddresses_.find(address) != localAddresses_.end();
}

bool AddressManager::IsNew(const IPAddress& address) const {
    std::unique_lock<std::recursive_mutex> lk(lock_);
    return newAddr_.find(address) != newAddr_.end();
}

bool AddressManager::IsOld(const IPAddress& address) const {
    std::unique_lock<std::recursive_mutex> lk(lock_);
    return oldAddr_.find(address) != oldAddr_.end();
}

void AddressManager::Clear() {
    std::unique_lock<std::recursive_mutex> lk(lock_);
    newAddr_.clear();
    oldAddr_.clear();
}

size_t AddressManager::SizeOfAllAddr() {
    std::unique_lock<std::recursive_mutex> lk(lock_);
    return newAddr_.size() + oldAddr_.size();
}
