#ifndef __SRC_INIT_H__
#define __SRC_INIT_H__

#include "caterpillar.h"
#include "config.h"
#include "cpptoml.h"
#include "cxxopts.hpp"
#include "file_utils.h"
#include "spdlog/sinks/basic_file_sink.h"

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
#endif // __SRC_INIT_H__
