#include "init.h"

std::unique_ptr<Config> config = std::make_unique<Config>();

void Init(int argc, char *argv[]) {
    // setup and parse the command line
    cxxopts::Options options("epic", "welcome to epic, enjoy your time!");
    try {
        SetupCommandline(options);
        std::cout << options.help() << std::endl;
        ParseCommandLine(argc, argv, options);
    } catch (const cxxopts::OptionException &e) {
        std::cerr << "error parsing options: " << e.what() << std::endl;
        exit(COMMANDLINE_INIT_FAILURE);
    }

    // load config file
    LoadConfigFile();

    // init logger
    InitLogger();
}

void SetupCommandline(cxxopts::Options &options) {
    options.add_options()
        ("c,configpath", "specified config path", cxxopts::value<std::string>()->default_value("config.toml"));
}

void ParseCommandLine(int argc, char **argv, cxxopts::Options &options) {
    auto result = options.parse(argc, argv);
    // since these two params have been set default values, there is no need to call result.count()
    // to detect if they have values
    config->setConfigFilePath(result["configpath"].as<std::string>());
}

void LoadConfigFile() {
    std::string config_path = config->getConfigFilePath();
    if (!CheckFileExist(config_path)) {
        std::cerr << "config.toml not found in current directory, will use the default config" << std::endl;
        return;
    }
    auto configContent = cpptoml::parse_file(config_path);
    auto log_config = configContent->get_table("logs");
    auto use_file_logger = log_config->get_as<bool>("use_file_logger").value_or(false);
    config->setUseFileLogger(use_file_logger);
    if (use_file_logger) {
        auto path = log_config->get_as<std::string>("path").value_or("./");
        auto filename = log_config->get_as<std::string>("filename").value_or("Debug.log");
        if (path[path.length() - 1] != '/') {
            path.append("/");
        }
        config->setLoggerFilename(filename);
        config->setLoggerPath(path);
    }
}

void InitLogger() {
    if (config->isUseFileLogger()) {
        UseFileLogger(config->getLoggerPath(), config->getLoggerFilename());
    }
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S] [%l] %v");
}

void UseFileLogger(const std::string &path, const std::string &filename) {
    try {
        if (!CheckDirExist(path)) {
            std::cerr << "The logger dir \"" << path << "\" not found, try to create the directory..." << std::endl;
            if (CheckFileExist(path)) {
                throw spdlog::spdlog_ex("File \"" + path + "\" existed");
            }
            if (Mkdir_recursive(path)) {
                std::cerr << path << " has been created" << std::endl;
            } else {
                throw spdlog::spdlog_ex("fail to create the logger file");
            }
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