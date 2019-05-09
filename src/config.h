#ifndef __SRC_CONFIG_H__
#define __SRC_CONFIG_H__

#include <memory>
#include <sstream>
#include <string>

#include "net_address.h"
#include "spdlog.h"

class Config {
public:
    // default bind ip address
    inline static const std::string defaultIP = "0.0.0.0";

    // default bind port
    static const uint16_t defaultPort = 7877;

    const std::string& GetConfigFilePath() const {
        return configFilePath_;
    }

    void SetConfigFilePath(const std::string& configFilePath) {
        configFilePath_ = configFilePath;
    }

    bool IsUseFileLogger() const {
        return useFileLogger_;
    }

    void SetUseFileLogger(bool useFileLogger) {
        useFileLogger_ = useFileLogger;
    }

    const std::string& GetLoggerPath() const {
        return loggerPath_;
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

    const std::string& GetAddressPath() const {
        return addressPath_;
    }

    void SetAddressPath(const std::string& addressPath) {
        addressPath_ = addressPath;
    }

    const std::string& GetAddressFilename() const {
        return addressFilename_;
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

    const std::string& GetDBPath() const {
        return dbPath_;
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

    void ShowConfig() {
        std::stringstream ss;
        ss << "current config: " << std::endl;
        ss << "config file path = " << configFilePath_ << std::endl;
        std::string usefilelogger = useFileLogger_ ? "true" : "false";
        ss << "use logger file = " << usefilelogger << std::endl;
        ss << "logger file path = " << loggerPath_ << loggerFilename_ << std::endl;
        ss << "saved address path = " << addressPath_ << addressFilename_ << std::endl;
        ss << "interval of saving address = " << saveInterval_ << " seconds" << std::endl;
        ss << "bind ip = " << bindAddress_ << std::endl;
        ss << "bind port = " << bindPort_ << std::endl;
        ss << "dbpath = " << dbPath_ << std::endl;
        ss << "seeds = [" << std::endl;

        for (const NetAddress& addr : seeds_) {
            ss << addr.ToString() << ',' << std::endl;
        }

        ss << "]" << std::endl;
        spdlog::info(ss.str());
    }

private:
    // config file
    std::string configFilePath_;

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

    std::vector<NetAddress> seeds_;

    // db
    std::string dbPath_ = "db/";
};

extern std::unique_ptr<Config> config;

#endif // __SRC_CONFIG_H__
