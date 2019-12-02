// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "epic-cli.h"

#include <ctime>
#include <iomanip>
#include <termios.h>

class ParsePairException : public std::exception {};

void PrintBanner() {
    std::string banner[] = {
        "******************************************************************",
        "        |                                                  |      ",
        "       / \\            ______ _____ _____ _____            / \\     ",
        "      / _ \\          |  ____|  __ \\_   _/ ____|          / _ \\    ",
        "     |.o '.|         | |__  | |__) || || |              |.o '.|   ",
        "     |'._.'|         |  __| |  ___/ | || |              |'._.'|   ",
        "     |     |         | |____| |    _| || |____          |     |   ",
        "   ,'|  |  |`.       |______|_|   |_____\\_____|       ,'|  |  |`. ",
        "  /  |  |  |  \\                                      /  |  |  |  \\",
        "  |,-'--|--'-.|                                      |,-'--|--'-.|",
        "*******************************************************************",
    };

    for (auto& line : banner) {
        std::cout << line << std::endl;
    }
}

std::vector<std::string> split(std::string input, char separator) {
    std::vector<std::string> output;
    while (!input.empty()) {
        auto pos = input.find(separator);
        output.emplace_back(input, 0, pos);
        if (pos == std::string::npos) {
            break;
        } else {
            input = input.substr(pos + 1);
        }
    }
    return output;
}

std::optional<std::string> get_pair_string(std::string& input) {
    auto left = input.find('{');
    if (left != std::string::npos) {
        auto right = input.find('}', left);
        if (right != std::string::npos) {
            auto result = std::string(input, left + 1, right - left - 1);
            input       = input.substr(right + 1);
            return result;
        } else {
            throw ParsePairException();
        }
    }
    return {};
}

template <typename P1, typename P2>
std::pair<P1, P2> get_pair(std::string& input) {
    auto str = split(input, ',');
    if (str.size() != 2) {
        throw ParsePairException();
    }

    return std::pair<P1, P2>(lexical_cast<P1>(str[0]), lexical_cast<P2>(str[1]));
}

template <typename P1, typename P2>
std::vector<std::pair<P1, P2>> parse_pair(std::string input) {
    std::vector<std::pair<P1, P2>> output;

    while (auto pair_str = get_pair_string(input)) {
        output.emplace_back(get_pair<P1, P2>(*pair_str));
    }

    return output;
}

int main(int argc, char** argv) {
    PrintBanner();
    EpicCli cli("epic");
    cli.Start();
    return 0;
}

EpicCli::EpicCli(std::string name) : exit_(false), name_(std::move(name)) {
    cli_     = std::make_unique<Cli>(CreateRootMenu(), [&, this](auto& out) { exit_ = true; });
    session_ = std::make_unique<CliLocalTerminalSession>(*cli_, std::cout);
}

std::string EpicCli::InputPassphrase(std::ostream& out) {
    std::string passphrase;
    while (passphrase.empty()) {
        std::getline(std::cin, passphrase);
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
    out << std::endl;
    return passphrase;
}

std::string EpicCli::GetLine() {
    std::string str;
    session_->ToStandardMode();
    std::getline(std::cin, str);
    session_->ToManualMode();
    return str;
}

std::optional<std::string> EpicCli::GetNewPassphrase(std::ostream& out) {
    out << "New Passphrase:";
    auto passphrase = InputPassphrase(out);
    out << "Confirm Passphrase:";
    auto confirm = InputPassphrase(out);
    if (passphrase != confirm) {
        out << "Your passphrases do not match" << std::endl;
        return {};
    }

    confirm.clear();
    return passphrase;
}


void EpicCli::Start() {
    while (!exit_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

std::unique_ptr<Menu> EpicCli::CreateRootMenu() {
    auto root = std::make_unique<Menu>(name_);
    root->Insert("open", [this](std::ostream& out, std::string host) { Open(out, host); }, "Connect to the rpc sever",
                 {"ip:port"});
    return root;
}

std::unique_ptr<Menu> EpicCli::CreateSubMenu(const std::string& title) {
    auto sub = std::make_unique<Menu>(title);
    sub->Insert(
        "close", [this](std::ostream& out) { Close(out); }, "Disconnect the rpc host");
    sub->Insert(
        "status", [this](std::ostream& out) { Status(out); }, "Show the peer status");
    sub->Insert(
        "start-miner", [this](std::ostream& out) { StartMiner(out); }, "Start miner");
    sub->Insert(
        "stop-miner", [this](std::ostream& out) { StopMiner(out); }, "Stop miner");
    sub->Insert(
        "generate-new-key", [this](std::ostream& out) { GenerateNewKey(out); }, "Generate the new key");
    sub->Insert("create-first-reg", [this](std::ostream& out, std::string address) { CreateFirstReg(out, address); },
                "Create the first registration before mining", {"the encoded address to receive miner reward"});
    sub->Insert("redeem", [this](std::ostream& out, std::string coins, std::string addr) { Redeem(out, coins, addr); },
                "Redeem miner rewards",
                {"the coin value to redeem (\"0\" or \"all\" to redeem the maximum value available)",
                 "the new address for the next redemption (enter \"new\" to generate a new key automatically)"});
    sub->Insert(
        "set-passphrase", [this](std::ostream& out) { SetPassphrase(out); }, "Set your new passphrase");
    sub->Insert(
        "change-passphrase", [this](std::ostream& out) { ChangePassphrase(out); }, "Change your passphrase");
    sub->Insert(
        "login", [this](std::ostream& out) { Login(out); }, "Login");
    sub->Insert(
        "get-balance", [this](std::ostream& out) { GetBalance(out); }, "Get the wallet balance");
    sub->Insert("connect", [this](std::ostream& out, std::string peers) { Connect(out, peers); },
                "Connect to the peers", {"ip:port,ip:port,..."});
    sub->Insert("disconnect", [this](std::ostream& out, std::string peers) { Disconnect(out, peers); },
                "Disconnect the peers sep", {"ip:port,ip:port,... or all"});
    sub->Insert("create-randomtx", [this](std::ostream& out, uint32_t num) { CreateRandomTx(out, num); },
                "Create random transactions for test", {"the num of transactions"});
    sub->Insert("create-tx",
                [this](std::ostream& out, uint64_t fee, std::string output_str) { CreateTx(out, fee, output_str); },
                "Create the transaction", {"the transaction fee", "{value,address},{value,address},..."});
    sub->Insert("show-peer", [this](std::ostream& out, std::string address) { ShowPeer(out, address); },
                "Show the peer information", {"ip:port or all"});
    return sub;
}

void EpicCli::Open(std::ostream& out, const std::string& host) {
    auto rpc = std::make_unique<RPCClient>(grpc::CreateChannel(host, grpc::InsecureChannelCredentials()));
    auto rsp = rpc->Status();
    if (rsp) {
        rpc_       = std::move(rpc);
        host_menu_ = cli_->RootMenu()->Insert(CreateSubMenu(host));
        cli_->RootMenu()->Exec({name_, host}, *session_);
        cli_->RootMenu()->Disable();
        out << "Succeed" << std::endl;
    } else {
        out << "Failed to connect the rpc host " << host << std::endl;
    }
}

void EpicCli::Close(std::ostream& out) {
    rpc_.reset();
    cli_->RootMenu()->Enable();
    cli_->RootMenu()->Exec({name_}, *session_);
    host_menu_.Remove();
}

void EpicCli::Status(std::ostream& out) {
    auto r = rpc_->Status();
    if (r) {
        out << *r << std::endl;
    } else {
        Close(out);
    }
}

void EpicCli::TryToMine(std::ostream& out) {
    auto r = rpc_->StartMiner();
    if (r) {
        out << ((*r) ? "OK" : "FAIL: Miner is already running") << std::endl;
    } else {
        Close(out);
    }
}

void EpicCli::StartMiner(std::ostream& out) {
    auto sync_completed = rpc_->SyncCompleted();
    if (!sync_completed) {
        Close(out);
    }

    if (*sync_completed) {
        TryToMine(out);
        return;
    }

    out << "The initial synchronization is not completed.\nWould you like to force the miner to "
           "start? [Y/N] ";
    while (true) {
        std::string yn = GetLine();

        if (yn == "Y" || yn == "y") {
            TryToMine(out);
            break;
        } else if (yn == "N" || yn == "n") {
            break;
        } else {
            out << "Would you like to force the miner to start? [Y/N] ";
        }
    }
}

void EpicCli::StopMiner(std::ostream& out) {
    auto r = rpc_->StopMiner();
    if (r) {
        out << *r << std::endl;
    } else {
        Close(out);
    }
}

void EpicCli::GenerateNewKey(std::ostream& out) {
    auto r = rpc_->GenerateNewKey();
    if (r) {
        out << "Address = " << (*r) << std::endl;
    } else {
        Close(out);
    }
}

void EpicCli::CreateFirstReg(std::ostream& out, std::string& address) {
    auto r = rpc_->CreateFirstReg(address);
    if (r) {
        if (r->empty()) {
            out << "Peer chain already exists. Would you like to start a new peer chain? [Y/N] ";

            while (true) {
                std::string yn = GetLine();

                if (yn == "Y" || yn == "y") {
                    out << "Address: (leave it empty if you would like to create a new key) ";
                    std::string addr = GetLine();

                    if (addr.empty()) {
                        auto rsp = rpc_->GenerateNewKey();
                        if (rsp) {
                            addr = *rsp;
                        } else {
                            Close(out);
                            return;
                        }
                    }

                    auto rsp = rpc_->CreateFirstReg(addr, true);
                    if (rsp) {
                        out << *rsp << std::endl;
                    } else {
                        Close(out);
                    }
                    break;

                } else if (yn == "N" || yn == "n") {
                    break;
                }

                out << "Wokld you like to start a new peer chain? [Y/N] ";
            }
        } else {
            out << *r << std::endl;
        }
    } else {
        Close(out);
    }
}

void EpicCli::SetPassphrase(std::ostream& out) {
    auto passphrase = GetNewPassphrase(out);
    if (!passphrase) {
        return;
    }

    if (auto r = rpc_->SetPassphrase(*passphrase)) {
        out << (*r) << std::endl;
    } else {
        Close(out);
    }
    passphrase->clear();
}

void EpicCli::ChangePassphrase(std::ostream& out) {
    out << "Old Passphrase:";
    auto oldPassphrase = InputPassphrase(out);
    auto newPassphrase = GetNewPassphrase(out);
    if (!newPassphrase) {
        return;
    }
    if (auto r = rpc_->ChangePassphrase(oldPassphrase, *newPassphrase)) {
        out << (*r) << std::endl;
    } else {
        Close(out);
    }
    newPassphrase->clear();
}

void EpicCli::Login(std::ostream& out) {
    out << "Passphrase:";
    auto passphrase = InputPassphrase(out);
    auto r          = rpc_->Login(passphrase);
    passphrase.clear();
    if (r) {
        out << (*r) << std::endl;
    } else {
        Close(out);
    }
}

void EpicCli::GetBalance(std::ostream& out) {
    auto r = rpc_->GetBalance();
    if (r) {
        out << (*r) << std::endl;
    } else {
        Close(out);
    }
}

void EpicCli::Connect(std::ostream& out, std::string& peers) {
    std::vector<std::string> addresses;

    addresses = split(peers, ',');

    auto r = rpc_->ConnectPeers(addresses);
    if (r) {
        out << (*r) << std::endl;
    } else {
        Close(out);
    }
}

void EpicCli::Disconnect(std::ostream& out, std::string& peers) {
    std::transform(peers.begin(), peers.end(), peers.begin(), [](unsigned char c) { return std::tolower(c); });
    if (peers == "all") {
        auto r = rpc_->DisconnectAllPeers();
        if (r) {
            out << (*r) << std::endl;
        } else {
            Close(out);
        }
    } else {
        std::vector<std::string> addresses;
        addresses = split(peers, ',');

        auto r = rpc_->DisconnectPeers(addresses);
        if (r) {
            out << (*r) << std::endl;
        } else {
            Close(out);
        }
    }
}

void EpicCli::Redeem(std::ostream& out, std::string& scoins, std::string& addr) {
    char* p;
    uint64_t coins = std::strtoul(scoins.c_str(), &p, 10);
    if (*p && scoins != "all") {
        out << "Invalid argument for coin value." << std::endl;
        return;
    }

    if (addr == "new") {
        addr.clear();
    }

    out << "Redeeming " << (coins ? std::to_string(coins) + " coin(s). " : "as much as possible. ")
        << (addr.empty() ? "Creating a new address for the next reg." : "The next reg address: " + addr) << std::endl;

    auto r = rpc_->Redeem(addr, coins);

    if (r) {
        out << *r << std::endl;
    } else {
        Close(out);
    }
}

void EpicCli::CreateRandomTx(std::ostream& out, uint32_t num) {
    auto r = rpc_->CreateRandomTx(num);
    if (r) {
        out << *r << std::endl;
    } else {
        Close(out);
    }
}

void EpicCli::CreateTx(std::ostream& out, uint64_t fee, std::string& output_str) {
    try {
        auto outputs = parse_pair<uint64_t, std::string>(output_str);

        out << "Total outputs:" << outputs.size() << " and fee:" << fee << std::endl;
        for (auto& output : outputs) {
            out << "output address:" << output.second << " coin:" << output.first << std::endl;
        }

        auto r = rpc_->CreateTx(outputs, fee);
        if (r) {
            out << (*r) << std::endl;
        } else {
            Close(out);
        }

    } catch (std::exception&) {
        out << "Failed to parse the second argument" << std::endl;
    }
}

void EpicCli::ShowPeer(std::ostream& out, std::string& address) {
    auto r = rpc_->ShowPeer(address);
    if (r) {
        out << *r << std::endl;
    } else {
        Close(out);
    }
}
