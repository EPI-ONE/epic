#include "init.h"

std::unique_ptr<Config> config;

void Init(int argc, char* argv[]) {
    config = std::make_unique<Config>();
    // setup and parse the command line
    cxxopts::Options options("epic", "welcome to epic, enjoy your time!");

    try {
        SetupCommandline(options);
        std::cout << options.help() << std::endl;
        ParseCommandLine(argc, argv, options);
    } catch (const cxxopts::OptionException& e) {
        std::cerr << "error parsing options: " << e.what() << std::endl;
        exit(COMMANDLINE_INIT_FAILURE);
    }

    // load config file
    LoadConfigFile();

    // init logger
    InitLogger();

    config->ShowConfig();
}

void SetupCommandline(cxxopts::Options& options) {
    // clang-format off
    options.add_options()
    ("c,configpath", "specified config path",cxxopts::value<std::string>()->default_value("config.toml"))
    ("b,bindip", "bind ip address",cxxopts::value<std::string>())
    ("p,bindport", "bind port", cxxopts::value<uint16_t>());
    // clang-format on
}

void ParseCommandLine(int argc, char** argv, cxxopts::Options& options) {
    auto result = options.parse(argc, argv);
    // since these two params have been set default values, there is no need to
    // call result.count() to detect if they have values
    config->SetConfigFilePath(result["configpath"].as<std::string>());
    if (result.count("bindip") > 0) {
        config->SetBindAddress(result["bindip"].as<std::string>());
    }

    if (result.count("bindport") > 0) {
        config->SetBindPort(result["bindport"].as<uint16_t>());
    }
}

void LoadConfigFile() {
    std::string config_path = config->GetConfigFilePath();
    if (!CheckFileExist(config_path)) {
        std::cerr << "config.toml not found in current directory, will use the "
                     "default config"
                  << std::endl;
        return;
    }

    auto configContent = cpptoml::parse_file(config_path);

    // logger
    auto log_config = configContent->get_table("logs");
    if (log_config) {
        auto use_file_logger = log_config->get_as<bool>("use_file_logger").value_or(false);
        config->SetUseFileLogger(use_file_logger);
        if (use_file_logger) {
            auto path     = log_config->get_as<std::string>("path").value_or("./");
            auto filename = log_config->get_as<std::string>("filename").value_or("Debug.log");

            if (path[path.length() - 1] != '/') {
                path.append("/");
            }

            config->SetLoggerFilename(filename);
            config->SetLoggerPath(path);
        }
    }

    // address manager
    auto address_config = configContent->get_table("address");
    if (address_config) {
        auto path     = address_config->get_as<std::string>("path").value_or("");
        auto filename = address_config->get_as<std::string>("filename").value_or("address.toml");
        auto interval = address_config->get_as<uint>("interval").value_or(15 * 60);
        config->SetAddressPath(path);
        config->SetAddressFilename(filename);
        config->SetSaveInterval(interval);
    }

    // network config
    auto network_config = configContent->get_table("network");
    if (network_config) {
        auto ip   = network_config->get_as<std::string>("ip");
        auto port = network_config->get_as<uint16_t>("port");
        if (ip && config->GetBindAddress() == config->defaultIP) {
            config->SetBindAddress(*ip);
        } else {
            spdlog::info("bind ip has been specified in the command line, discard the ip in the config file");
        }

        if (port && config->GetBindPort() == config->defaultPort) {
            config->SetBindPort(*port);
        } else {
            spdlog::info("bind port has been specified in the command line, discard the port in the config file");
        }
    }

    // seeds
    auto seeds = configContent->get_table_array("seeds");
    if (seeds) {
        for (const auto& seed : *seeds) {
            auto ip   = seed->get_as<std::string>("ip");
            auto port = seed->get_as<uint16_t>("port");

            if (ip && port) {
                config->AddSeeds(*ip, *port);
            }
        }
    }

    auto db_config = configContent->get_table("db");
    if (db_config) {
        auto db_path = db_config->get_as<std::string>("path");
        if (db_path) {
            config->SetDBPath(*db_path);
        }
    }
}

void InitLogger() {
    if (config->IsUseFileLogger()) {
        UseFileLogger(config->GetLoggerPath(), config->GetLoggerFilename());
    }
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S] [%l] %v");
}

void UseFileLogger(const std::string& path, const std::string& filename) {
    try {
        if (!CheckDirExist(path)) {
            std::cerr << "The logger dir \"" << path << "\" not found, try to create the directory..." << std::endl;
            if (Mkdir_recursive(path)) {
                std::cerr << path << " has been created" << std::endl;
            } else {
                throw spdlog::spdlog_ex("fail to create the logger file");
            }
        }

        auto file_logger = spdlog::basic_logger_mt("basic_logger", path + filename);
        spdlog::set_default_logger(file_logger);
    } catch (const spdlog::spdlog_ex& ex) {
        std::cerr << "The file logger init failed: " << ex.what() << std::endl;
        std::cerr << "Please check your config setting" << std::endl;
        exit(LOG_INIT_FAILURE);
    }
}
