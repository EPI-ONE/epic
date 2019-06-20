#include "init.h"
#include "dag_manager.h"
#include "rpc_server.h"
#include <atomic>
#include <net/peer_manager.h>
#include <signal.h>

std::unique_ptr<Config> config;

Block GENESIS;
NodeRecord GENESIS_RECORD;
std::unique_ptr<Caterpillar> CAT;
std::unique_ptr<DAGManager> DAG;
std::unique_ptr<PeerManager> peer_manager;
std::unique_ptr<RPCServer> rpc_server;
static std::atomic_bool b_shutdown = false;
typedef void (*signal_handler_t)(int);

static void KickShutdown(int) {
    b_shutdown = true;
}

static void RegisterSignalHandler(int signal, signal_handler_t handler) {
    struct sigaction sa;
    sa.sa_handler = handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(signal, &sa, nullptr);
}

void InitSignal() {
    RegisterSignalHandler(SIGTERM, KickShutdown);
    RegisterSignalHandler(SIGINT, KickShutdown);

    signal(SIGPIPE, SIG_IGN);
}

void CreateRoot(const std::string& path) {
    if (!CheckDirExist(path)) {
        if (Mkdir_recursive(path)) {
            spdlog::info("root {} has been created", path);
        } else {
            throw spdlog::spdlog_ex("fail to create the path" + path);
        }
    }
}

void Init(int argc, char* argv[]) {
    config = std::make_unique<Config>();
    // setup and parse the command line
    cxxopts::Options options("epic", "welcome to epic, enjoy your time!");

    try {
        SetupCommandline(options);
        ParseCommandLine(argc, argv, options);
    } catch (const cxxopts::OptionException& e) {
        std::cout << options.help() << std::endl;
        std::cerr << "error parsing options: " << e.what() << std::endl;
        exit(COMMANDLINE_INIT_FAILURE);
    }

    // load config file
    LoadConfigFile();

    // init logger
    InitLogger();

    InitSignal();

    config->ShowConfig();

    // set global variables
    // TODO: add argument parsing
    try {
        SelectParams(ParamsType::TESTNET);
    } catch (const std::invalid_argument& err) {
        std::cerr << "error choosing params: " << err.what() << std::endl;
    }

    CAT          = std::make_unique<Caterpillar>(config->GetDBPath());
    DAG          = std::make_unique<DAGManager>();
    peer_manager = std::make_unique<PeerManager>();
    if (!(config->GetDisableRPC())) {
        std::string rpc_addr_str = "0.0.0.0:" + std::to_string(config->GetRPCPort());
        auto rpcAddress          = NetAddress::GetByIP(rpc_addr_str);
        rpc_server               = std::make_unique<RPCServer>(*rpcAddress);
    }
}

void SetupCommandline(cxxopts::Options& options) {
    // clang-format off
    options.add_options()
    ("h,help", "print this message", cxxopts::value<bool>())
    ("c,configpath", "specified config path",cxxopts::value<std::string>()->default_value("config.toml"))
    ("b,bindip", "bind ip address",cxxopts::value<std::string>())
    ("p,bindport", "bind port", cxxopts::value<uint16_t>())
    ("connect", "connect", cxxopts::value<std::string>())
    ("disable-rpc", "disable rpc server", cxxopts::value<bool>())
    ;
    // clang-format on
}

void ParseCommandLine(int argc, char** argv, cxxopts::Options& options) {
    auto result = options.parse(argc, argv);
    if (result["help"].as<bool>()) {
        std::cout << options.help() << std::endl;
        exit(0);
    }
    // since these two params have been set default values, there is no need to
    // call result.count() to detect if they have values
    config->SetConfigFilePath(result["configpath"].as<std::string>());
    if (result.count("bindip") > 0) {
        config->SetBindAddress(result["bindip"].as<std::string>());
    }

    if (result.count("bindport") > 0) {
        config->SetBindPort(result["bindport"].as<uint16_t>());
    }
    if (result.count("connect") > 0) {
        config->SetConnect(result["connect"].as<std::string>());
    }
    config->SetDisableRPC(result["disable-rpc"].as<bool>());
}

void LoadConfigFile() {
    std::string config_path = config->GetConfigFilePath();
    if (!CheckFileExist(config_path)) {
        std::cerr << "config.toml not found in current directory, will use the default config" << std::endl;
        return;
    }

    auto configContent = cpptoml::parse_file(config_path);

    auto global_config = configContent->get_table("global");
    if (global_config) {
        auto root = global_config->get_as<std::string>("root");
        if (root) {
            config->SetRoot(*root);
        }
    }

    CreateRoot(config->GetRoot());

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
    auto dns_seeds = configContent->get_table_array("dns_seeds");
    if (dns_seeds) {
        for (const auto& seed : *dns_seeds) {
            auto host = seed->get_as<std::string>("hostname");
            auto port = seed->get_as<uint16_t>("port");

            if (host && port) {
                config->AddSeedByDNS(*host, *port);
            }
        }

        if (config->GetSeedSize() == 0) {
            auto ip_seeds = configContent->get_table_array("ip_seeds");
            for (const auto& seed : *dns_seeds) {
                auto ip   = seed->get_as<std::string>("ip");
                auto port = seed->get_as<uint16_t>("port");
                if (ip && port) {
                    config->AddSeedByIP(*ip, *port);
                }
            }
        }
    }

    // db
    auto db_config = configContent->get_table("db");
    if (db_config) {
        auto db_path = db_config->get_as<std::string>("path");
        if (db_path) {
            config->SetDBPath(*db_path);
        }
    }

    // rpc
    auto rpc_config = configContent->get_table("rpc");
    if (rpc_config) {
        auto rpc_port = rpc_config->get_as<uint16_t>("port");
        if (rpc_port) {
            config->SetRPCPort(*rpc_port);
        }
    }
}

void InitLogger() {
    if (config->IsUseFileLogger()) {
        UseFileLogger(config->GetLoggerPath(), config->GetLoggerFilename());
    }
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e][%t][%l] %v");
    spdlog::set_level(spdlog::level::info);
    spdlog::flush_on(spdlog::level::info);
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

bool Start() {
    if (!peer_manager->Init(config)) {
        return false;
    }
    peer_manager->Start();
    if (!(config->GetDisableRPC())) {
        rpc_server->Start();
    }
    return true;
}

void ShutDown() {
    spdlog::info("shutdown start");
    peer_manager->Stop();
    if (!(config->GetDisableRPC())) {
        rpc_server->Shutdown();
    }
    CAT.reset();
    spdlog::info("shutdown finish");
    spdlog::shutdown();
}

void WaitShutdown() {
    while (!b_shutdown) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}
