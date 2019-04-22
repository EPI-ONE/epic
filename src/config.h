#ifndef EPIC_CONFIG_H
#define EPIC_CONFIG_H
#include <memory>
#include <string>

class Config {
private:
    // config file
    std::string configFilePath;

    // logger
    bool useFileLogger         = true;
    std::string loggerPath     = "logs/";
    std::string loggerFilename = "Debug.log";

public:
    const std::string& getConfigFilePath() const {
        return configFilePath;
    }
    void setConfigFilePath(const std::string& configFilePath) {
        Config::configFilePath = configFilePath;
    }
    bool isUseFileLogger() const {
        return useFileLogger;
    }
    void setUseFileLogger(bool useFileLogger) {
        Config::useFileLogger = useFileLogger;
    }
    const std::string& getLoggerPath() const {
        return loggerPath;
    }
    void setLoggerPath(const std::string& loggerPath) {
        Config::loggerPath = loggerPath;
    }
    const std::string& getLoggerFilename() const {
        return loggerFilename;
    }
    void setLoggerFilename(const std::string& loggerFilename) {
        Config::loggerFilename = loggerFilename;
    }
};

extern std::unique_ptr<Config> config;
#endif // EPIC_CONFIG_H
