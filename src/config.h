#ifndef EPIC_CONFIG_H
#define EPIC_CONFIG_H
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

    std::vector<NetAddress> seeds;

public:
    const std::string& getConfigFilePath() const {
        return configFilePath_;
    }
    void setConfigFilePath(const std::string& configFilePath) {
        configFilePath_ = configFilePath;
    }
    bool isUseFileLogger() const {
        return useFileLogger_;
    }
    void setUseFileLogger(bool useFileLogger) {
        useFileLogger_ = useFileLogger;
    }
    const std::string& getLoggerPath() const {
        return loggerPath_;
    }
    void setLoggerPath(const std::string& loggerPath) {
        loggerPath_ = loggerPath;
    }
    const std::string& getLoggerFilename() const {
        return loggerFilename_;
    }
    void setLoggerFilename(const std::string& loggerFilename) {
        loggerFilename_ = loggerFilename;
    }
    const std::string& getAddressPath() const {
        return addressPath_;
    }
    void setAddressPath(const std::string& addressPath) {
        addressPath_ = addressPath;
    }
    const std::string& getAddressFilename() const {
        return addressFilename_;
    }
    void setAddressFilename(const std::string& addressFilename) {
        addressFilename_ = addressFilename;
    }
    uint32_t getSaveInterval() const {
        return saveInterval_;
    }

    void setSaveInterval(uint32_t saveInterval) {
        Config::saveInterval_ = saveInterval;
    }

    const std::string& getBindAddress() const {
        return bindAddress_;
    }

    void setBindAddress(const std::string& bindAddress) {
        bindAddress_ = bindAddress;
    }

    uint16_t getBindPort() const {
        return bindPort_;
    }

    void setBindPort(uint16_t bindPort) {
        bindPort_ = bindPort;
    }

    void addSeeds(const std::string& ip, const uint16_t& port) {
        auto address = NetAddress::StringToNetAddress(ip, port);
        if (address) {
            seeds.push_back(*address);
        }
    }

    std::vector<NetAddress> getSeeds() const {
        return seeds;
    }

    void showConfig() {
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
        ss << "seeds = [" << std::endl;
        for (const NetAddress& addr : seeds) {
            ss << addr.ToString() << ',' << std::endl;
        }
        ss << "]" << std::endl;
        spdlog::info(ss.str());
    }
};

extern std::unique_ptr<Config> config;
#endif // EPIC_CONFIG_H
