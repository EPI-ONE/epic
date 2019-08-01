#include "cxxopts.hpp"
#include "rpc_client.h"
#include "spdlog/sinks/basic_file_sink.h"

enum : uint8_t {
    NORMAL_EXIT = 0,
    COMMANDLINE_INIT_FAILURE,
    LOG_INIT_FAILURE,
    PARAMS_INIT_FAILURE,
};
class UnconnectedException : public std::exception {};

RPCClient CreateClient(std::string ip, uint16_t port) {
    return RPCClient(grpc::CreateChannel(ip + ":" + std::to_string(port), grpc::InsecureChannelCredentials()));
}

int main(int argc, char** argv) {
    std::string command;
    std::string arg_value;
    uint16_t rpc_port;
    // clang-format off
    cxxopts::Options options("epicc", "epic client");
    options.positional_help("COMMAND. available commands: status, start-miner, stop-miner, create-tx, stop").show_positional_help();

    options
    .add_options("command")
    ("command", "Command", cxxopts::value<std::string>(command)->default_value(""))
    ("value","Value",cxxopts::value<std::string>(arg_value)->default_value(""))
    ;

    options.add_options()
    ("h,help", "print this message", cxxopts::value<bool>())

    ("rpc-port", "client rpc port which is used to connect to the server", cxxopts::value<uint16_t>(rpc_port)->default_value("3777"))
    ;

    options.parse_positional({"command" ,"value"});
    // clang-format on
    try {
        auto parsed_options = options.parse(argc, argv);
        if (parsed_options["help"].as<bool>()) {
            std::cout << options.help() << std::endl;
            return NORMAL_EXIT;
        }
        auto client = CreateClient("0.0.0.0", rpc_port);

        if (command == "status") {
            auto r = client.Status();
            if (r.has_value()) {
                std::cout << "Miner status: " << (r.value().isminerrunning() ? "RUNNING" : "NOT RUNNING") << std::endl;
                std::cout << "Latest milestone hash: " << r.value().latestmshash().hash() << std::endl;
            } else {
                throw UnconnectedException();
            }
        } else if (command == "stop") {
            if (client.Stop()) {
                std::cout << "OK";
            } else {
                throw UnconnectedException();
            }
        } else if (command == "start-miner") {
            auto r = client.StartMiner();
            if (r.has_value()) {
                std::cout << ((*r) ? "OK" : "FAIL: Miner is already running") << std::endl;
            } else {
                throw UnconnectedException();
            }
        } else if (command == "stop-miner") {
            auto r = client.StopMiner();
            if (r.has_value()) {
                std::cout << ((*r) ? "OK" : "FAIL: Miner is already stopped") << std::endl;
            } else {
                throw UnconnectedException();
            }
        } else if (command == "create-tx") {
            size_t size = 0;
            if (!arg_value.empty()) {
                size = std::stoi(arg_value);
            }
            auto r = client.CreateTx(size);
            if (r.has_value()) {
                std::cout << *r << std::endl;
            } else {
                throw UnconnectedException();
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
    } catch (UnconnectedException& e) {
        std::cout << "No epic is running on " << std::to_string(rpc_port) << " port" << std::endl;
    }
    return NORMAL_EXIT;
}
