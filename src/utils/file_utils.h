#ifndef EPIC_FILE_UTILS_H
#define EPIC_FILE_UTILS_H
#include <fstream>
#include <iostream>
#include <sys/types.h>
#include <sys/stat.h>

inline bool CheckPathExist(const std::string &path) {
    struct stat info;
    return stat(path.c_str(), &info) == 0;
}


// used c like way to implement this
// TODO later can try to use c++17 std::filesystem to implement this
void Mkdir_recursive(const std::string &path) {
    char pathArray[256];
    char *p = nullptr;
    snprintf(pathArray, sizeof(pathArray), "%s", path.c_str());
    size_t len = strlen(pathArray);
    if (pathArray[len - 1] == '/')
        pathArray[len - 1] = 0;
    for (p = pathArray; *p; p++)
        if (*p == '/') {
            *p = 0;
            mkdir(pathArray, S_IRWXU);
            *p = '/';
        }
    mkdir(pathArray, S_IRWXU);
}
#endif //EPIC_FILE_UTILS_H
