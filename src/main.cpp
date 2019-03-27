#include <string>
#include <iostream>

#include "hash.h"
#include "block.h"
#include "file_utils.h"
#include "crypto/sha256.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/spdlog.h"
#include "tinyformat.h"
#include "transaction.h"
#include "utils/cpptoml.h"
#include "arith_uint256.h"


class uint256;
//class arith_uint256;

enum {
    LOG_INIT_FAILURE = 1,
};

void UseFileLogger(const cpptoml::option<std::string> &path, const cpptoml::option<std::string> &filename) {
    std::string path_ = path ? *path : "./";
    if (path_[path_.length() - 1] != '/') {
        path_.append("/");
    }
    std::string filename_ = filename ? *filename : "Debug.log";
    try {
        if (!CheckDirExist(path_)) {
            std::cerr << "The logger dir \"" << path_ << "\" not found, try to create the directory..." << std::endl;
            if (CheckFileExist(path_)) {
                throw spdlog::spdlog_ex("File \"" + path_ + "\" existed");
            }
            if (Mkdir_recursive(path_)) {
                std::cerr << path_ << " has been created" << std::endl;
            } else {
                throw spdlog::spdlog_ex("fail to create the logger file");
            }
        }
        auto file_logger = spdlog::basic_logger_mt("basic_logger", path_ + filename_);
        spdlog::set_default_logger(file_logger);
    }
    catch (const spdlog::spdlog_ex &ex) {
        std::cerr << "The file logger init failed: " << ex.what() << std::endl;
        std::cerr << "Please check your config setting" << std::endl;
        exit(LOG_INIT_FAILURE);
    }
}

void LoadConfigFile() {
    std::string config_path = "config.toml";
    if (!CheckFileExist(config_path)) {
        std::cerr << "config.toml not found in bin/, use the default config.toml" << std::endl;
        config_path = "../config.toml";
    }
    auto config = cpptoml::parse_file(config_path);
    auto log_config = config->get_table("logs");
    auto use_file_logger = log_config->get_as<bool>("use_file_logger");
    if (use_file_logger && *use_file_logger) {
        auto path = log_config->get_as<std::string>("path");
        auto filename = log_config->get_as<std::string>("filename");
        UseFileLogger(path, filename);
    }
}

int main() {
    LoadConfigFile();
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S] [%l] %v");

    spdlog::info("Welcome to epic, enjoy your time!");

    uint256 zeros = uint256S(new char[256]());
    TxOutPoint outpoint = TxOutPoint(zeros, 1);
    TxInput input = TxInput(outpoint, Script());
    TxOutput output = TxOutput(100, Script());
    Transaction tx = Transaction(1);
    tx.AddInput(input);
    tx.AddOutput(output);

    arith_uint256 zeroos = UintToArith256(zeros);
    std::cout << strprintf("A null sha: \n %s", zeroos.ToString()) << std::endl;

    std::cout << strprintf("A transaction: \n %s", tx.ToString()) << std::endl;
}
