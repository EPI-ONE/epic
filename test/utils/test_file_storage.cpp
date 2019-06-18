#include <gtest/gtest.h>

#include <filesystem>
#include <iostream>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

class TestFileStorage : public testing::Test {
};

TEST_F(TestFileStorage, try_syntax) {

    fs::path pathToShow("/usr/local/include/");
    std::cout << "path =" << pathToShow << '\n';

    auto currentp = fs::current_path();
    std::cout << "Current path: " << currentp << std::endl;

    /*std::string dir= "sandbox/a/b";
    fs::create_directories(dir);

    std::ofstream("sandbox/file1.txt");
    fs::path symPath= fs::current_path() /=  "sandbox";
    symPath /= "syma";
    fs::create_symlink("a", "symPath");
    
    std::cout << "fs::is_directory(dir): " << fs::is_directory(dir) << std::endl;
    std::cout << "fs::exists(symPath): "  << fs::exists(symPath) << std::endl;
    std::cout << "fs::symlink(symPath): " << fs::is_symlink(symPath) << std::endl;
    

    for(auto& p: fs::recursive_directory_iterator("sandbox"))
        std::cout << p.path() << std::endl;*/
}
