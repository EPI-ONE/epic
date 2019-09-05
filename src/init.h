// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef EPIC_INIT_H
#define EPIC_INIT_H

#include "config.h"
#include "cpptoml.h"
#include "cxxopts.hpp"
#include "file_utils.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "storage.h"

extern std::atomic_bool b_shutdown;
extern ECCVerifyHandle handle;

enum : uint8_t {
    NORMAL_EXIT = 0,
    COMMANDLINE_INIT_FAILURE,
    LOG_INIT_FAILURE,
    PARAMS_INIT_FAILURE,
    DAG_INIT_FAILURE,
};

int Init(int argc, char* argv[]);

bool Start();
void ShutDown();
void WaitShutdown();

void LoadConfigFile();

void SetupCommandline(cxxopts::Options& options);
void ParseCommandLine(int argc, char* argv[], cxxopts::Options& options);

void UseFileLogger(const std::string& path, const std::string& filename);
void InitLogger();

void CreateDaemon();
#endif // EPIC_INIT_H
