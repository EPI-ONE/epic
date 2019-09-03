// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef __SRC_CONFIG_H__
#define __SRC_CONFIG_H__

#include "net_address.h"
#include "spdlog.h"

#include <memory>
#include <sstream>
#include <string>

class Config {
public:
    // default bind ip address
    inline static const std::string defaultIP = "0.0.0.0";

    // default bind port
    static const uint16_t defaultPort = 7877;

    // default rpc port
    static const uint16_t defaultRPCPort = 3777;

    const std::string& GetConfigFilePath() const {
        return configFilePath_;
    }

    void SetConfigFilePath(const std::string& configFilePath) {
        configFilePath_ = configFilePath;
    }

    void SetRoot(const std::string& root) {
        root_ = root;
        if (root_.back() != '/') {
            root_.append("/");
        }
    }

    const std::string& GetRoot() const {
        return root_;
    }

    bool IsUseFileLogger() const {
        return useFileLogger_;
    }

    void SetUseFileLogger(bool useFileLogger) {
        useFileLogger_ = useFileLogger;
    }

    const std::string GetLoggerPath() const {
        return GetRoot() + loggerPath_;
    }

    void SetLoggerPath(const std::string& loggerPath) {
        loggerPath_ = loggerPath;
    }

    const std::string& GetLoggerFilename() const {
        return loggerFilename_;
    }

    void SetLoggerFilename(const std::string& loggerFilename) {
        loggerFilename_ = loggerFilename;
    }

    const std::string GetAddressPath() const {
        return GetRoot() + addressPath_;
    }

    void SetAddressPath(const std::string& addressPath) {
        addressPath_ = addressPath;
    }

    const std::string GetAddressFilename() const {
        return GetRoot() + addressFilename_;
    }

    void SetAddressFilename(const std::string& addressFilename) {
        addressFilename_ = addressFilename;
    }

    uint32_t GetSaveInterval() const {
        return saveInterval_;
    }

    void SetSaveInterval(uint32_t saveInterval) {
        Config::saveInterval_ = saveInterval;
    }

    const std::string& GetBindAddress() const {
        return bindAddress_;
    }

    void SetBindAddress(const std::string& bindAddress) {
        bindAddress_ = bindAddress;
    }

    uint16_t GetBindPort() const {
        return bindPort_;
    }

    void SetBindPort(uint16_t bindPort) {
        bindPort_ = bindPort;
    }

    std::string GetNetworkType() const {
        return networkType_;
    }

    void SetNetworkType(const std::string& networkType) {
        networkType_ = networkType;
    }

    std::string GetDBPath() const {
        return GetRoot() + dbPath_;
    }

    void SetDBPath(const std::string& dbPath) {
        dbPath_ = dbPath;
    }

    void AddSeedByIP(const std::string& ip, const uint16_t& port) {
        auto address = NetAddress::GetByIP(ip, port);
        if (address) {
            seeds_.push_back(*address);
        }
    }

    void AddSeedByDNS(const std::string& hostname, const uint16_t& port) {
        auto address = NetAddress::GetByHostname(hostname, port);
        if (address) {
            seeds_.push_back(*address);
        }
    }

    std::vector<NetAddress> GetSeeds() const {
        return seeds_;
    }

    size_t GetSeedSize() const {
        return seeds_.size();
    }

    void SetDisableRPC(bool flag) {
        disableRPC_ = flag;
    }

    bool GetDisableRPC() const {
        return disableRPC_;
    }

    void SetRPCPort(uint16_t port) {
        rpcPort_ = port;
    }

    uint16_t GetRPCPort() const {
        return rpcPort_;
    }

    bool IsDaemon() const {
        return daemon_;
    }

    void SetDaemon(bool daemon) {
        daemon_ = daemon;
    }

    bool IsStartWithNewDB() const {
        return startWithNewDB;
    }
    void SetStartWithNewDB(bool startWithNewDb) {
        startWithNewDB = startWithNewDb;
    }

    void ShowConfig() {
        std::stringstream ss;
        ss << std::endl << "current config: " << std::endl;
        ss << "config file path = " << GetConfigFilePath() << std::endl;
        ss << "use logger file = " << useFileLogger_ << std::endl;
        ss << "logger file path = " << GetLoggerPath() << loggerFilename_ << std::endl;
        ss << "saved address path = " << GetAddressPath() << addressFilename_ << std::endl;
        ss << "interval of saving address = " << saveInterval_ << " seconds" << std::endl;
        ss << "bind ip = " << bindAddress_ << std::endl;
        ss << "bind port = " << bindPort_ << std::endl;
        ss << "network type = " << networkType_ << std::endl;
        ss << "dbpath = " << GetDBPath() << std::endl;
        ss << "disable rpc = " << (disableRPC_ ? "yes" : "no") << std::endl;
        ss << "rpc port = " << rpcPort_ << std::endl;
        ss << "seeds = [" << std::endl;
        ss << "wallet path = " << GetWalletPath() << " with backup period " << GetWalletBackup() << std::endl;

        for (const NetAddress& addr : seeds_) {
            ss << addr.ToString() << ',' << std::endl;
        }

        ss << "]" << std::endl;
        spdlog::info(ss.str());
    }

    void SetConnect(const std::string& connect) {
        connect_ = connect;
    }

    const std::string& GetConnect() const {
        return connect_;
    }

    void SetWalletPath(const std::string& wallet) {
        walletPath_ = wallet;
    }

    std::string GetWalletPath() const {
        return GetRoot() + walletPath_;
    }

    void SetWalletBackup(uint32_t backup) {
        backupPeriod_ = backup;
    }

    uint32_t GetWalletBackup() {
        return backupPeriod_;
    }

private:
    // config file
    std::string configFilePath_;
    std::string root_ = "data/";

    // logger
    bool useFileLogger_         = true;
    std::string loggerPath_     = "logs/";
    std::string loggerFilename_ = "Debug.log";

    // address manager
    std::string addressPath_     = "";
    std::string addressFilename_ = "address.toml";
    std::uint32_t saveInterval_  = 15 * 60;

    // network config
    std::string bindAddress_ = defaultIP;
    uint16_t bindPort_       = defaultPort;
    std::string connect_;
    std::string networkType_ = "Testnet";
    std::vector<NetAddress> seeds_;

    // db
    bool startWithNewDB = false;
    std::string dbPath_ = "db/";

    // rpc
    bool disableRPC_;
    uint16_t rpcPort_ = defaultRPCPort;

    // wallet
    std::string walletPath_ = "wallet/";
    uint32_t backupPeriod_;


    // daemon
    bool daemon_;
};

extern std::unique_ptr<Config> CONFIG;

#endif // __SRC_CONFIG_H__
