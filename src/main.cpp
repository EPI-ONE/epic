#include <iostream>
#include <sstream>
#include <string>

#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "net/peer_manager.h"
#include "net/net_address.h"
#include "utils/cpptoml.h"
//#include <gtest/gtest.h>


// here the path should be "../". I didn't use create dir function because the header file
// is different between Windows and Unix
void Use_FileLogger(const std::string& path,const std::string& filename){
    try
    {
        auto file_logger = spdlog::basic_logger_mt("basic_logger", path+filename);
        spdlog::set_default_logger(file_logger);
    }
    catch (const spdlog::spdlog_ex &ex)
    {
        std::cout << "Log init failed: " << ex.what() << std::endl;
    }
}

void Load_Config_File(){
    auto config = cpptoml::parse_file("config.toml");
    auto log_config = config->get_table("logs");
    auto use_file_logger = log_config->get_as<bool>("use_file_logger");
    if(use_file_logger){
        auto path = log_config->get_as<std::string>("path");
        auto filename = log_config->get_as<std::string>("filename");
        Use_FileLogger(*path,*filename);
    }
}


int main(){
    Load_Config_File();
    spdlog::info("Welcome to epic, enjoy your time!");
    std::string ip_s = "192.168.0.1:7877";
    NetAddress ip(ip_s);
    spdlog::info("For test: the ip address is {}",ip.ToString());
}