#include <iostream>
#include <string>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <utils/cpptoml.h>
#include <file_utils.h>

enum {
    LOG_INIT_FAILURE = 1,
    UNDEFINED_FAILURE
};

void Use_FileLogger(const std::string &path, const std::string &filename) {
    try {
        if (!CheckPathExist(path)) {
            std::cerr << path << " not found, create the directory" << std::endl;
            Mkdir_recursive(path);
        }
        auto file_logger = spdlog::basic_logger_mt("basic_logger", path + filename);
        spdlog::set_default_logger(file_logger);
    }
    catch (const spdlog::spdlog_ex &ex) {
        std::cerr << "The file logger init failed: " << ex.what() << std::endl;
        std::cerr << "Please check your config setting" << std::endl;
        exit(LOG_INIT_FAILURE);
    }
}

void Load_Config_File() {
    std::string config_path = "config.toml";
    if (!CheckPathExist(config_path)) {
        std::cerr << "config.toml not found in bin/, use the default config.toml" << std::endl;
        config_path = "../default_config.toml";
    }
    auto config = cpptoml::parse_file(config_path);
    auto log_config = config->get_table("logs");
    auto use_file_logger = log_config->get_as<bool>("use_file_logger");
    if (*use_file_logger) {
        auto path = log_config->get_as<std::string>("path");
        auto filename = log_config->get_as<std::string>("filename");
        Use_FileLogger(*path, *filename);
    }
}

int main() {
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S] [%l] %v");
    Load_Config_File();
    spdlog::info("Welcome to epic, enjoy your time!");
}