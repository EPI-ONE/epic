#ifndef EPIC_INIT_H
#define EPIC_INIT_H

#include "config.h"
#include "cpptoml.h"
#include "cxxopts.hpp"
#include "file_utils.h"
#include "spdlog/sinks/basic_file_sink.h"

enum {
    COMMANDLINE_INIT_FAILURE = 1,
    LOG_INIT_FAILURE         = 2,
};

void Init(int argc, char* argv[]);

void LoadConfigFile();

void SetupCommandline(cxxopts::Options& options);
void ParseCommandLine(int argc, char* argv[], cxxopts::Options& options);

void UseFileLogger(const std::string& path, const std::string& filename);
void InitLogger();


#endif // EPIC_INIT_H
