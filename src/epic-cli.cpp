// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "cxxopts.h"
#include "rpc_client.h"
#include "spdlog/spdlog.h"

#include <termios.h>
#include <unistd.h>

enum : uint8_t {
    NORMAL_EXIT = 0,
    COMMANDLINE_INIT_FAILURE,
    LOG_INIT_FAILURE,
    PARAMS_INIT_FAILURE,
};

class UnconnectedException : public std::exception {};

enum CommandType : uint8_t {
    STATUS = 0,
    START_MINER,
    STOP_MINER,
    STOP,
    CREATE_RANDOM_TX,
    GENERATE_NEWKEY,
    GET_BALANCE,
    CREATE_TX,
    SET_PASSPHRASE,
    CHANGE_PASSPHRASE,
    LOGIN,
    DISCONNECTPEER,
    CONNECTPEERS,
    DISCONNECTALLPEERS,
    INVALID
};

RPCClient CreateClient(std::string ip, uint16_t port) {
    return RPCClient(grpc::CreateChannel(ip + ":" + std::to_string(port), grpc::InsecureChannelCredentials()));
}

std::unordered_map<std::string, CommandType> InitCommand() {
    std::unordered_map<std::string, CommandType> command{

        {"status", CommandType::STATUS},
        {"start-miner", CommandType::START_MINER},
        {"stop-miner", CommandType::STOP_MINER},
        {"stop", CommandType::STOP},
        {"create-randomtx", CommandType::CREATE_RANDOM_TX},
        {"generate-newkey", CommandType::GENERATE_NEWKEY},
        {"get-balance", CommandType::GET_BALANCE},
        {"create-tx", CommandType::CREATE_TX},
        {"set-passphrase", CommandType::SET_PASSPHRASE},
        {"change-passphrase", CommandType::CHANGE_PASSPHRASE},
        {"login", CommandType::LOGIN},
        {"disconnect-peer", CommandType ::DISCONNECTPEER},
        {"disconnect-all", CommandType::DISCONNECTALLPEERS},
        {"connect-peers", CommandType ::CONNECTPEERS}};
    return command;
}

void SetStdinEcho(bool enable = true) {
    struct termios tty;
    tcgetattr(STDIN_FILENO, &tty);
    if (!enable) {
        tty.c_lflag &= ~ECHO;
    } else {
        tty.c_lflag |= ECHO;
    }
    (void) tcsetattr(STDIN_FILENO, TCSANOW, &tty);
}

std::optional<std::string> GetNewPassphrase() {
    std::string passphrase;
    std::string comfirm;
    std::cout << "Passphrase:";
    SetStdinEcho(false);
    std::cin >> passphrase;
    std::cout << "\nComfirm Passphrase:";
    std::cin >> comfirm;
    SetStdinEcho(true);
    std::cout << std::endl;

    if (passphrase != comfirm) {
        return {};
    }

    comfirm.clear();
    return passphrase;
}

int main(int argc, char** argv) {
    std::string command;
    std::string arg_value;
    uint16_t rpc_port;
    const auto commandMap = InitCommand();
    // clang-format off
    cxxopts::Options options("epic-cli", "epic client");
    options.positional_help(
            "COMMAND \n\n"
            "Available commands: \n"
            "   1. status \n"
            "   2. start-miner \n"
            "   3. stop-miner \n"
            "   4. stop \n"
            "   5. create-randomtx      args=size: uint64 (the size of transactions you want to create) \n"
            "   6. generate-newkey \n"
            "   7. get-balance \n"
            "   8. create-tx            args=fee: uint64 (optional, default to be 1),   \n"
            "                                outputValue1: uint64, outputAddr1: string, \n"
            "                                outputValue2: uint64, outputAddr2: string, \n"
            "                                ...\n"
            "   9. set-passphrase       arg=string: your new passphrase \n"
            "   10. change-passphrase   args=string: your old passphrase, your new passphrase \n"
            "   11. login               arg=string: your current passphrase \n"
            "   12. disconnect-all \n"
            "   13. connect-peers       arg=array of string: your specified address string (ip + port)\n"
            "   14. disconnect-peer     arg=string: network address to disconnect (ip + port)\n"
            "\nAvailable options:"
            ).show_positional_help();

    std::vector<std::string> more_args;
    options
    .add_options("command")
    ("command", "Command", cxxopts::value<std::string>(command)->default_value(""))
    ("value", "Value", cxxopts::value<std::string>(arg_value)->default_value(""))
    ("more_value", "more argruments", cxxopts::value<std::vector<std::string>>(more_args))
    ;

    options.add_options()
    ("h, help", "print this message", cxxopts::value<bool>())

    ("rpc-port", "client rpc port which is used to connect to the server", cxxopts::value<uint16_t>(rpc_port)->default_value("3777"))
    ;

    options.parse_positional({"command", "value", "more_value"});
    // clang-format on
    try {
        auto parsed_options = options.parse(argc, argv);
        if (parsed_options["help"].as<bool>()) {
            std::cout << options.help() << std::endl;
            return NORMAL_EXIT;
        }
        auto client = CreateClient("0.0.0.0", rpc_port);

        switch (commandMap.at(command)) {
            case STATUS: {
                auto r = client.Status();
                if (r.has_value()) {
                    std::cout << "Miner status: " << (r.value().isminerrunning() ? "RUNNING" : "NOT RUNNING")
                              << std::endl;
                    std::cout << "Latest milestone hash: " << r.value().latestmshash().hash() << std::endl;
                } else {
                    throw UnconnectedException();
                }
                break;
            }

            case STOP: {
                if (client.Stop()) {
                    std::cout << "OK" << std::endl;
                } else {
                    throw UnconnectedException();
                }
                break;
            }

            case START_MINER: {
                auto r = client.StartMiner();
                if (r.has_value()) {
                    std::cout << ((*r) ? "OK" : "FAIL: Miner is already running") << std::endl;
                } else {
                    throw UnconnectedException();
                }
                break;
            }

            case STOP_MINER: {
                auto r = client.StopMiner();
                if (r.has_value()) {
                    std::cout << *r << std::endl;
                } else {
                    throw UnconnectedException();
                }
                break;
            }

            case CREATE_RANDOM_TX: {
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
                break;
            }

            case GET_BALANCE: {
                auto r = client.GetBalance();
                if (r) {
                    std::cout << (*r) << std::endl;
                } else {
                    throw UnconnectedException();
                }
                break;
            }

            case GENERATE_NEWKEY: {
                auto r = client.GenerateNewKey();
                if (r) {
                    std::cout << (*r) << std::endl;
                } else {
                    throw UnconnectedException();
                }
                break;
            }

            case CREATE_TX: {
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
                break;
            }
            case CONNECTPEERS: {
                std::vector<std::string> addresses;

                if (arg_value.empty()) {
                    std::cout << "please specify at least one address";
                    return PARAMS_INIT_FAILURE;
                }
                addresses.push_back(arg_value);
                for (auto addr : more_args) {
                    addresses.emplace_back(addr);
                }
                auto r = client.ConnectPeers(addresses);
                if (r) {
                    std::cout << (*r) << std::endl;
                } else {
                    throw UnconnectedException();
                }
                break;
            }
            case DISCONNECTALLPEERS: {
                auto r = client.DisconnectAllPeers();
                if (r) {
                    std::cout << (*r) << std::endl;
                } else {
                    throw UnconnectedException();
                }
                break;
            }
            case DISCONNECTPEER: {
                auto r = client.DisconnectPeer(arg_value);
                if (r) {
                    std::cout << (*r) << std::endl;
                } else {
                    throw UnconnectedException();
                }
                break;
            }
            case SET_PASSPHRASE: {
                if (!arg_value.empty()) {
                    return PARAMS_INIT_FAILURE;
                }

                auto passphrase = GetNewPassphrase();
                if (!passphrase.has_value()) {
                    std::cout << "Your passphrases do not match" << std::endl;
                    break;
                }

                if (auto r = client.SetPassphrase(*passphrase)) {
                    std::cout << (*r) << std::endl;
                } else {
                    throw UnconnectedException();
                }
                passphrase->clear();
                break;
            }

            case CHANGE_PASSPHRASE: {
                if (!arg_value.empty()) {
                    return PARAMS_INIT_FAILURE;
                }

                std::string oldPassphrase;
                std::cout << "Old Passphrase:";
                SetStdinEcho(false);
                std::cin >> oldPassphrase;
                std::cout << std::endl;
                auto newPassphrase = GetNewPassphrase();
                if (!newPassphrase.has_value()) {
                    std::cout << "Your new passphrases do not match" << std::endl;
                    break;
                }
                if (auto r = client.ChangePassphrase(oldPassphrase, *newPassphrase)) {
                    std::cout << (*r) << std::endl;
                } else {
                    throw UnconnectedException();
                }
                newPassphrase->clear();
                break;
            }

            case LOGIN: {
                if (!arg_value.empty()) {
                    return PARAMS_INIT_FAILURE;
                }

                std::string passphrase;
                std::cout << "Passphrase:";
                SetStdinEcho(false);
                std::cin >> passphrase;
                SetStdinEcho(true);
                std::cout << std::endl;
                auto r = client.Login(passphrase);
                passphrase.clear();
                if (r) {
                    std::cout << (*r) << std::endl;
                } else {
                    throw UnconnectedException();
                }
                break;
            }

            default: {
                std::cout << options.help() << std::endl;
                std::cerr << "please specify one of the commands" << std::endl;
                exit(COMMANDLINE_INIT_FAILURE);
            }
        }

    } catch (const std::out_of_range& e) {
        std::cout << options.help() << std::endl;
        std::cerr << "please specify one of the commands" << std::endl;
        exit(COMMANDLINE_INIT_FAILURE);
    } catch (const cxxopts::OptionException& e) {
        std::cout << options.help() << std::endl;
        std::cerr << "error parsing options: " << e.what() << std::endl;
        exit(COMMANDLINE_INIT_FAILURE);
    } catch (UnconnectedException& e) {
        std::cout << "No epic is running on " << std::to_string(rpc_port) << " port" << std::endl;
    }
    return NORMAL_EXIT;
}
