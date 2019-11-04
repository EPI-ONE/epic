// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "basic_rpc_server.h"
#include "cxxopts.h"
#include "service/rpc_service.h"
#include "service/solver.h"
#include "solver_manager.h"

#include <csignal>
static std::atomic_bool shutdown = false;

typedef void (*signal_handler_t)(int);

static void Shutdown(int) {
    shutdown = true;
}

static void RegisterSignalHandler(int signal, signal_handler_t handler) {
    struct sigaction sa;
    sa.sa_handler = handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(signal, &sa, nullptr);
}

static void InitSignalHandle() {
    RegisterSignalHandler(SIGTERM, Shutdown);
    RegisterSignalHandler(SIGINT, Shutdown);

    signal(SIGPIPE, SIG_IGN);
}

int main(int argc, char* argv[]) {
    InitSignalHandle();
    cxxopts::Options options("solver server");
    std::string address{};
    uint32_t thread_size{0};
    options.add_options()("h,help", "print help message", cxxopts::value<bool>())(
        "addr", "ip address with port", cxxopts::value<std::string>())("size", "max size of threads",
                                                                       cxxopts::value<uint32_t>());

    auto parsed_options = options.parse(argc, argv);
    if (parsed_options["help"].as<bool>()) {
        std::cout << options.help() << std::endl;
        return 0;
    }
    if (parsed_options["addr"].count() > 0) {
        address = parsed_options["addr"].as<std::string>();
    }
    if (parsed_options["size"].count() > 0) {
        thread_size = parsed_options["size"].as<uint32_t>();
    }
    if (address.empty() || thread_size == 0) {
        spdlog::error("please specify both ip address and size of threads");
        spdlog::info("{}", options.help());
        return -1;
    }

    spdlog::info("Creating RPC server. IP address = {}", address);
    auto server = std::make_unique<BasicRPCServer>(address);

    spdlog::info("Creating solver. Thread size = {}", thread_size);
    auto solver = std::make_shared<SolverManager>(thread_size);

    auto service = std::make_unique<SolverRPCServiceImpl>(solver);
    server->Start(std::vector<grpc::Service*>{service.get()});
    solver->Start();

    sleep(2);
    while (server->IsRunning() && !shutdown) {
        sleep(1);
    }
    solver->Stop();
    server->Shutdown();
}
