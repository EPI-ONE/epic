#ifndef __SRC_INIT_H__
#define __SRC_INIT_H__

#include "config.h"
#include "cpptoml.h"
#include "cxxopts.hpp"
#include "file_utils.h"
#include "params.h"
#include "spdlog/sinks/basic_file_sink.h"

enum : uint8_t {
    COMMANDLINE_INIT_FAILURE = 1,
    LOG_INIT_FAILURE,
};

void Init(int argc, char* argv[]);

void LoadConfigFile();

void SetupCommandline(cxxopts::Options& options);
void ParseCommandLine(int argc, char* argv[], cxxopts::Options& options);

void UseFileLogger(const std::string& path, const std::string& filename);
void InitLogger();

#endif // __SRC_INIT_H__
