// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "cxxopts.h"
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
    options.positional_help("COMMAND \n\n"
                            "Available commands: \n"
                            "   1. status \n"
                            "   2. start-miner \n"
                            "   3. stop-miner \n"
                            "   4. stop \n"
                            "   5. create-randomtx, args=size: uint64 (the size of transactions you want to create) \n"
                            "   6. generate-newkey \n"
                            "   7. get-balance \n"
                            "   8. create-tx args=fee: uint64 (optional, default to be 1), \\\n"
                            "                     outputValue1: uint64, outputAddr1: string, \\\n"
                            "                     outputValue2: uint64, outputAddr2: string, \\\n"
                            "                     ...\n\n"
                            "Available options:"
                            ).show_positional_help();

    std::vector<std::string> more_args;
    options
    .add_options("command")
    ("command", "Command", cxxopts::value<std::string>(command)->default_value(""))
    ("value","Value",cxxopts::value<std::string>(arg_value)->default_value(""))
    ("more_value","more argruments",cxxopts::value<std::vector<std::string>>(more_args))
    ;

    options.add_options()
    ("h,help", "print this message", cxxopts::value<bool>())

    ("rpc-port", "client rpc port which is used to connect to the server", cxxopts::value<uint16_t>(rpc_port)->default_value("3777"))
    ;


    options.parse_positional({"command" ,"value","more_value"});
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
        } else if (command == "create-randomtx") {
            size_t size = 0;
            if (!arg_value.empty()) {
                size = std::stoi(arg_value);
            }
            auto r = client.CreateRandomTx(size);
            if (r.has_value()) {
                std::cout << *r << std::endl;
            } else {
                throw UnconnectedException();
            }
        } else if (command == "get-balance") {
            auto r = client.GetBalance();
            if (r) {
                std::cout << (*r) << std::endl;
            } else {
                throw UnconnectedException();
            }
        } else if (command == "generate-newkey") {
            auto r = client.GenerateNewKey();
            if (r) {
                std::cout << (*r) << std::endl;
            } else {
                throw UnconnectedException();
            }
        } else if (command == "create-tx") {
            std::vector<std::pair<uint64_t, std::string>> outputs;
            uint64_t fee;

            if (more_args.empty()) {
                std::cout << "please specify at least one output";
                return PARAMS_INIT_FAILURE;
            }

            if (more_args.size() % 2 == 0) {
                fee = std::stoi(arg_value);
                std::cout << "You have specified the transaction fee = " + arg_value << std::endl;
                for (int i = 0, j = i + 1; j < more_args.size(); i = i + 2, j = j + 2) {
                    outputs.emplace_back(std::make_pair(std::stoi(more_args[i]), more_args[j]));
                }
            } else {
                fee = 0;
                std::cout << "You didn't specify the transaction fee, will use default fee" << std::endl;
                for (int i = 1, j = i + 1; i <= more_args.size(); i = i + 2, j = j + 2) {
                    if (i == 1) {
                        outputs.emplace_back(std::make_pair(std::stoi(arg_value), more_args[0]));
                    } else {
                        outputs.emplace_back(std::make_pair(std::stoi(more_args[i]), more_args[j]));
                    }
                }
            }
            auto r = client.CreateTx(outputs, fee);
            if (r) {
                std::cout << (*r) << std::endl;
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
