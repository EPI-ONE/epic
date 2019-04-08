#include "file_utils.h"

bool CheckDirExist(const std::string& dirPath) {
    struct stat info;
    if (stat(dirPath.c_str(), &info) != 0)
        return false;
    return info.st_mode & S_IFDIR;
}

bool CheckFileExist(const std::string& filePath) {
    std::ifstream f(filePath.c_str());
    return f.good();
}

// used c like way to implement this
// TODO later can try to use c++17 std::filesystem to implement this
bool Mkdir_recursive(const std::string &path) {
    if (path.empty()) {
        return true;
    }
    char pathArray[1024];
    errno = 0;
    if (path.length() > sizeof(pathArray) - 1) {
        errno = ENAMETOOLONG;
        return false;
    }
    snprintf(pathArray, sizeof(pathArray), "%s", path.c_str());
    size_t len = strlen(pathArray);

    if (pathArray[len - 1] == '/')
        pathArray[len - 1] = 0;
    for (char* p = pathArray; *p; p++)
        if (*p == '/') {
            *p = 0;
            if (mkdir(pathArray, S_IRWXU) != 0) {
                if (errno != EEXIST)
                    return false;
            }
            *p = '/';
        }
    if (mkdir(pathArray, S_IRWXU) != 0) {
        if (errno != EEXIST)
            return false;
    }
    return true;
}