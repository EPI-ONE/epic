#ifndef EPIC_FILE_UTILS_H
#define EPIC_FILE_UTILS_H

#include <cerrno>
#include <fstream>
#include <iostream>
#include <limits.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

bool CheckDirExist(const std::string& dirPath);

bool CheckFileExist(const std::string& filePath);

// used c like way to implement this
bool Mkdir_recursive(const std::string& path);

#endif // EPIC_FILE_UTILS_H
