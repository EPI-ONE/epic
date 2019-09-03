// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "init.h"
#include "dag_manager.h"
#include "mempool.h"
#include "peer_manager.h"
#include "rpc_server.h"
#include "wallet.h"

#include <atomic>
#include <csignal>
#include <spawn.h>

Block GENESIS;
Vertex GENESIS_VERTEX;
std::unique_ptr<Config> CONFIG;
std::unique_ptr<PeerManager> PEERMAN;
std::unique_ptr<BlockStore> STORE;
std::unique_ptr<DAGManager> DAG;
std::unique_ptr<RPCServer> RPC;
std::unique_ptr<Miner> MINER;
std::shared_ptr<Wallet> WALLET;
std::unique_ptr<MemPool> MEMPOOL;

ECCVerifyHandle handle;

std::atomic_bool b_shutdown = false;

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
        if (MkdirRecursive(path)) {
            spdlog::info("root {} has been created", path);
        } else {
            throw spdlog::spdlog_ex("fail to create the path " + path);
        }
    }
}

int Init(int argc, char* argv[]) {
    std::cout << "Start initializing...\n" << std::endl;

    /*
     *  Create config instance
     */
    CONFIG = std::make_unique<Config>();

    /*
     *  Setup and parse the command line
     */
    cxxopts::Options options("epic", "welcome to epic, enjoy your time!");

    try {
        SetupCommandline(options);
        ParseCommandLine(argc, argv, options);
    } catch (const cxxopts::OptionException& e) {
        std::cout << options.help() << std::endl;
        std::cerr << "error parsing options: " << e.what() << std::endl;
        return COMMANDLINE_INIT_FAILURE;
    }

    /*
     *  Load config file
     */
    LoadConfigFile();

    /*
     * Init logger
     */
    InitLogger();

    CONFIG->ShowConfig();

    /*
     * Init signal and register handle functions
     */
    InitSignal();

    /*
     * Set global variables
     */
    const std::map<std::string, ParamsType> parseType = {
        {"Mainnet", ParamsType::MAINNET}, {"Testnet", ParamsType::TESTNET}, {"Unittest", ParamsType::UNITTEST}};
    try {
        SelectParams(parseType.at(CONFIG->GetNetworkType()));
    } catch (const std::out_of_range& err) {
        std::cerr << "wrong format of network type in config.toml" << std::endl;
        return PARAMS_INIT_FAILURE;
    } catch (const std::invalid_argument& err) {
        std::cerr << "error choosing params: " << err.what() << std::endl;
        return PARAMS_INIT_FAILURE;
    }

    file::SetDataDirPrefix(CONFIG->GetRoot());

    /*
     * Load caterpillar, dag and memory pool
     */
    if (CONFIG->IsStartWithNewDB()) {
        // delete old block data
        DeleteDir(CONFIG->GetDBPath());
        DeleteDir(CONFIG->GetRoot() + file::typestr[file::FileType::BLK]);
        DeleteDir(CONFIG->GetRoot() + file::typestr[file::FileType::VTX]);
        DeleteDir(CONFIG->GetWalletPath());
    }

    STORE = std::make_unique<BlockStore>(CONFIG->GetDBPath());

    if (!STORE->DBExists(GENESIS.GetHash())) {
        // put genesis block into cat
        std::vector<VertexPtr> genesisLvs = {std::make_shared<Vertex>(GENESIS_VERTEX)};
        STORE->StoreLevelSet(genesisLvs);
    }

    DAG = std::make_unique<DAGManager>();
    if (!DAG->Init()) {
        return DAG_INIT_FAILURE;
    }

    /*
     * Load wallet
     */
    WALLET = std::make_shared<Wallet>(CONFIG->GetWalletPath(), CONFIG->GetWalletBackup());
    DAG->RegisterOnLvsConfirmedCallback(std::bind(&Wallet::OnLvsConfirmed, WALLET, std::placeholders::_1,
                                                  std::placeholders::_2, std::placeholders::_3));

    MEMPOOL = std::make_unique<MemPool>();

    /*
     * Create network instance
     */
    PEERMAN = std::make_unique<PeerManager>();

    /*
     * Initialize ECC
     */
    ECC_Start();
    handle = ECCVerifyHandle();

    /*
     * Initialize miner
     */
    MINER = std::make_unique<Miner>();

    /*
     * Create rpc instance
     */
    if (!(CONFIG->GetDisableRPC())) {
        auto rpcAddress = NetAddress::GetByIP("0.0.0.0:" + std::to_string(CONFIG->GetRPCPort()));
        RPC             = std::make_unique<RPCServer>(*rpcAddress);
    }

    std::cout << "Finish initializing...\n" << std::endl;
    return NORMAL_EXIT;
}

void SetupCommandline(cxxopts::Options& options) {
    // clang-format off
    options.add_options()
    ("h,help", "print this message", cxxopts::value<bool>())
    ("configpath", "specified config path", cxxopts::value<std::string>()->default_value("config.toml"))
    ("b,bindip", "bind ip address", cxxopts::value<std::string>())
    ("p,bindport", "bind port", cxxopts::value<uint16_t>())
    ("connect","connect", cxxopts::value<std::string>())
    ("disable-rpc", "disable rpc server", cxxopts::value<bool>())
    ("D,daemon","make the program running in a daemon process", cxxopts::value<bool>())
    ("N,newdb","start with the new db", cxxopts::value<bool>())
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
    CONFIG->SetConfigFilePath(result["configpath"].as<std::string>());
    if (result.count("bindip") > 0) {
        CONFIG->SetBindAddress(result["bindip"].as<std::string>());
    }

    if (result.count("bindport") > 0) {
        CONFIG->SetBindPort(result["bindport"].as<uint16_t>());
    }
    if (result.count("connect") > 0) {
        CONFIG->SetConnect(result["connect"].as<std::string>());
    }
    if (result.count("daemon") > 0) {
        CONFIG->SetDaemon(result["daemon"].as<bool>());
    }
    if (result.count("newdb") > 0) {
        CONFIG->SetStartWithNewDB(true);
    }
    CONFIG->SetDisableRPC(result["disable-rpc"].as<bool>());
}

void LoadConfigFile() {
    std::string config_path = CONFIG->GetConfigFilePath();
    if (!CheckFileExist(config_path)) {
        std::cerr << "config.toml not found in current directory, will use the default config" << std::endl;
        return;
    }

    auto configContent = cpptoml::parse_file(config_path);

    auto global_config = configContent->get_table("global");
    if (global_config) {
        auto root = global_config->get_as<std::string>("root");
        if (root) {
            CONFIG->SetRoot(*root);
        }
    }

    CreateRoot(CONFIG->GetRoot());

    // logger
    auto log_config = configContent->get_table("logs");
    if (log_config) {
        auto use_file_logger = log_config->get_as<bool>("use_file_logger").value_or(false);
        CONFIG->SetUseFileLogger(use_file_logger);
        if (use_file_logger) {
            auto path     = log_config->get_as<std::string>("path").value_or("./");
            auto filename = log_config->get_as<std::string>("filename").value_or("Debug.log");

            if (path[path.length() - 1] != '/') {
                path.append("/");
            }

            CONFIG->SetLoggerFilename(filename);
            CONFIG->SetLoggerPath(path);
        }
    }

    // address manager
    auto address_config = configContent->get_table("address");
    if (address_config) {
        auto path     = address_config->get_as<std::string>("path").value_or("");
        auto filename = address_config->get_as<std::string>("filename").value_or("address.toml");
        auto interval = address_config->get_as<uint>("interval").value_or(15 * 60);
        CONFIG->SetAddressPath(path);
        CONFIG->SetAddressFilename(filename);
        CONFIG->SetSaveInterval(interval);
    }

    // network config
    auto network_config = configContent->get_table("network");
    if (network_config) {
        auto ip          = network_config->get_as<std::string>("ip");
        auto port        = network_config->get_as<uint16_t>("port");
        auto networkType = network_config->get_as<std::string>("type");
        if (ip && CONFIG->GetBindAddress() == CONFIG->defaultIP) {
            CONFIG->SetBindAddress(*ip);
        } else {
            spdlog::info("bind ip has been specified in the command line, discard the ip in the config file");
        }

        if (port && CONFIG->GetBindPort() == CONFIG->defaultPort) {
            CONFIG->SetBindPort(*port);
        } else {
            spdlog::info("bind port has been specified in the command line, discard the port in the config file");
        }
        CONFIG->SetNetworkType(*networkType);
    }

    // seeds
    auto dns_seeds = configContent->get_table_array("dns_seeds");
    if (dns_seeds) {
        for (const auto& seed : *dns_seeds) {
            auto host = seed->get_as<std::string>("hostname");
            auto port = seed->get_as<uint16_t>("port");

            if (host && port) {
                CONFIG->AddSeedByDNS(*host, *port);
            }
        }

        if (CONFIG->GetSeedSize() == 0) {
            auto ip_seeds = configContent->get_table_array("ip_seeds");
            for (const auto& seed : *dns_seeds) {
                auto ip   = seed->get_as<std::string>("ip");
                auto port = seed->get_as<uint16_t>("port");
                if (ip && port) {
                    CONFIG->AddSeedByIP(*ip, *port);
                }
            }
        }
    }

    // db
    auto db_config = configContent->get_table("db");
    if (db_config) {
        auto db_path = db_config->get_as<std::string>("path");
        if (db_path) {
            CONFIG->SetDBPath(*db_path);
        }
    }

    // rpc
    auto rpc_config = configContent->get_table("rpc");
    if (rpc_config) {
        auto rpc_port = rpc_config->get_as<uint16_t>("port");
        if (rpc_port) {
            CONFIG->SetRPCPort(*rpc_port);
        }
    }

    // wallet
    auto wallet_config = configContent->get_table("wallet");
    if (wallet_config) {
        auto wallet_path = wallet_config->get_as<std::string>("path");
        if (wallet_path) {
            CONFIG->SetWalletPath(*wallet_path);
        }

        auto wallet_backup = wallet_config->get_as<uint32_t>("backup_period");
        if (wallet_backup) {
            CONFIG->SetWalletBackup(*wallet_backup);
        }
    }
}

void InitLogger() {
    if (CONFIG->IsUseFileLogger()) {
        UseFileLogger(CONFIG->GetLoggerPath(), CONFIG->GetLoggerFilename());
    }
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e][%t][%l] %v");
    spdlog::set_level(spdlog::level::debug);
    spdlog::flush_on(spdlog::level::debug);
}

void UseFileLogger(const std::string& path, const std::string& filename) {
    try {
        if (!CheckDirExist(path)) {
            std::cerr << "The logger dir \"" << path << "\" not found, try to create the directory..." << std::endl;
            if (MkdirRecursive(path)) {
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
    // start p2p network
    if (!PEERMAN->Init(CONFIG)) {
        std::cerr << "fail to start peer manager" << std::endl;
        return false;
    }
    PEERMAN->Start();
    WALLET->Start();
    if (!WALLET->GenerateMaster()) {
        std::cerr << "fail to generate master key for wallet" << std::endl;
        return false;
    } // TODO: make it from rpc calls

    // start rpc server
    if (!(CONFIG->GetDisableRPC())) {
        RPC->Start();
    }
    return true;
}

void ShutDown() {
    spdlog::info("shutdown start");

    if (!(CONFIG->GetDisableRPC())) {
        RPC->Shutdown();
    }

    PEERMAN->Stop();
    WALLET->Stop();
    DAG->Stop();
    STORE->Stop();
    if (MINER->IsRunning()) {
        MINER->Stop();
    }

    STORE.reset();
    DAG.reset();
    MEMPOOL.reset();
    PEERMAN.reset();
    MINER.reset();

    ECC_Stop();
    handle.~ECCVerifyHandle();

    spdlog::info("shutdown finish");
    spdlog::shutdown();
}

void WaitShutdown() {
    while (!b_shutdown) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void CreateDaemon() {
    if (CONFIG->IsDaemon()) {
        std::cout << "Create daemon process, parent process exit" << std::endl;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        daemon(1, 0);
#pragma clang diagnostic pop
    }
}
