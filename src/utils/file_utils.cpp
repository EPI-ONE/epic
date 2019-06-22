#include "file_utils.h"
#include "tinyformat.h"

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
bool Mkdir_recursive(const std::string& path) {
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

std::string file::GetPath(FileType type, uint32_t epoch) {
    return std::string{prefixes[type] + "epoch" + tfm::format("%05d", epoch)};
}

std::string file::GetFileName(FileType type, uint32_t name) {
    return std::string{typestr[type] + tfm::format("%05d", name) + ".dat"};
}

std::string std::to_string(FileReader& freader) {
    std::string s;
    s += strprintf("Reading file %s at position %i.", freader.GetFileName(), freader.GetOffset());
    return s;
}

std::string std::to_string(FileWriter& fwriter) {
    std::string s;
    s += strprintf("Writing file %s at position %i.", fwriter.GetFileName(), fwriter.GetOffset());
    return s;
}
