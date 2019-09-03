// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "cxxopts.hpp"
#include "storage.h"
#include "toml_specifacation.h"

#include <iostream>

int ParseArg(int argc, char** argv, std::string& root, std::string& type) {
    cxxopts::Options options("tools", "epic tools");
    options.positional_help("Please specify root path and network type").show_positional_help();

    // clang-format off
    options.add_options()
    ("h,help", "print this message", cxxopts::value<bool>())
    ("r,root", "root path of data, example: data",cxxopts::value<std::string>(root))
    ("t,type", "network type, one of Mainnet, Testnet and Unittest",cxxopts::value<std::string>(type));
    // clang-format on

    try {
        auto parsed_options = options.parse(argc, argv);
        if (parsed_options["help"].as<bool>()) {
            std::cout << options.help() << std::endl;
            return -1;
        }
        if (root.empty() || type.empty()) {
            throw cxxopts::OptionException("Please specify the params");
        }
    } catch (const cxxopts::OptionException& e) {
        std::cerr << "error parsing options: " << e.what() << std::endl;
        std::cout << options.help() << std::endl;
        return -1;
    }
    return 0;
}

int main(int argc, char** argv) {
    std::string rootpath;
    std::string net_type;
    auto init_res = ParseArg(argc, argv, rootpath, net_type);
    if (!init_res) {
        const std::map<std::string, ParamsType> parseType = {
            {"Mainnet", ParamsType::MAINNET}, {"Testnet", ParamsType::TESTNET}, {"Unittest", ParamsType::UNITTEST}};
        try {
            SelectParams(parseType.at(net_type));
        } catch (const std::out_of_range& err) {
            std::cerr << "wrong format of network type" << std::endl;
            return -1;
        } catch (const std::invalid_argument& err) {
            std::cerr << "error choosing params: " << err.what() << std::endl;
            return -1;
        }
        file::SetDataDirPrefix(rootpath);
        STORE = std::make_unique<BlockStore>(rootpath + "/db/");

        auto height = STORE->GetHeadHeight();
        for (int i = 1; i < height; i++) {
            std::ofstream file;
            std::string dir = "tools/records/";
            if (!CheckDirExist(dir)) {
                Mkdir_recursive(dir);
            }
            file.open(dir + std::to_string(i) + ".toml", std::ios::out | std::ios::trunc);

            auto set = STORE->GetLevelSetRecsAt(i);
            auto ms  = STORE->GetMilestoneAt(i);
            auto res = LvsWithRecToToml(set);
            file << *res;
            file.close();
        }

        STORE.reset();
    }
}
