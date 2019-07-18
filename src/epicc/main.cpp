#include "cxxopts.hpp"
#include "rpc_client.h"
#include "spdlog/sinks/basic_file_sink.h"

enum : uint8_t {
    NORMAL_EXIT = 0,
    COMMANDLINE_INIT_FAILURE,
    LOG_INIT_FAILURE,
    PARAMS_INIT_FAILURE,
};

int main(int argc, char** argv) {
    std::string command;
    uint16_t rpc_port;
    // clang-format off
    cxxopts::Options options("epicc", "epic client");
    options.positional_help("COMMAND. available commands: status, start-miner, stop-miner, stop").show_positional_help();

    options
    .add_options("command")
    ("command", "Command", cxxopts::value<std::string>(command)->default_value(""))
    ;

    options.add_options()
    ("h,help", "print this message", cxxopts::value<bool>())

    ("rpc-port", "client rpc port which is used to connect to the server", cxxopts::value<uint16_t>(rpc_port)->default_value("3777"))
    ;

    options.parse_positional({"command"});
    // clang-format on
    try {
        auto parsed_options = options.parse(argc, argv);
        if (parsed_options["help"].as<bool>()) {
            std::cout << options.help() << std::endl;
            return NORMAL_EXIT;
        }

        if (command == "status") {
            RPCClient client(
                grpc::CreateChannel("0.0.0.0:" + std::to_string(rpc_port), grpc::InsecureChannelCredentials()));
            auto r = client.Status();
            if (r.has_value()) {
                std::cout << "Miner status: " << (r.value().isminerrunning() ? "RUNNING" : "NOT RUNNING") << std::endl;
                std::cout << "Latest milestone hash: " << r.value().latestmshash().hash() << std::endl;
            } else {
                std::cout << "No epic is running on " << std::to_string(rpc_port) << " port" << std::endl;
            }
        } else if (command == "stop") {
            RPCClient client(
                grpc::CreateChannel("0.0.0.0:" + std::to_string(rpc_port), grpc::InsecureChannelCredentials()));
            if (client.Stop()) {
                std::cout << "OK";
            } else {
                std::cout << "No epic is running on " << std::to_string(rpc_port) << " port" << std::endl;
            }
        } else if (command == "start-miner") {
            RPCClient client(
                grpc::CreateChannel("0.0.0.0:" + std::to_string(rpc_port), grpc::InsecureChannelCredentials()));
            auto r = client.StartMiner();
            if (r.has_value()) {
                std::cout << ((*r) ? "OK" : "FAIL: Miner is already running") << std::endl;
            } else {
                std::cout << "No epic is running on " << std::to_string(rpc_port) << " port" << std::endl;
            }
        } else if (command == "stop-miner") {
            RPCClient client(
                grpc::CreateChannel("0.0.0.0:" + std::to_string(rpc_port), grpc::InsecureChannelCredentials()));
            auto r = client.StopMiner();
            if (r.has_value()) {
                std::cout << ((*r) ? "OK" : "FAIL: Miner is already stopped") << std::endl;
            } else {
                std::cout << "No epic is running on " << std::to_string(rpc_port) << " port" << std::endl;
            }
        } else {
            std::cout << options.help() << std::endl;
            std::cerr << "please specify one of the commands" << std::endl;
            exit(COMMANDLINE_INIT_FAILURE);
        }
    } catch (const cxxopts::OptionException& e) {
        std::cout << options.help() << std::endl;
        std::cerr << "error parsing options: " << e.what() << std::endl;
        exit(COMMANDLINE_INIT_FAILURE);
    }
    return NORMAL_EXIT;
}
