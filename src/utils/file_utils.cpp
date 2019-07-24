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
        if (*p == '/' && p != pathArray) {
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

void DeleteDir(const std::string& dirpath) {
    if (CheckDirExist(dirpath)) {
        std::string cmd = "rm -r " + dirpath;
        system(cmd.c_str());
    }
}

void file::SetDataDirPrefix(std::string strprefix) {
    prefix = strprefix + "/";
}

std::string file::GetEpochPath(FileType type, uint32_t epoch) {
    return std::string{prefix + typestr[type] + "/E" + tfm::format("%06d", epoch)};
}

std::string file::GetFileName(FileType type, uint32_t name) {
    return std::string{typestr[type] + tfm::format("%06d", name) + ".dat"};
}

std::string file::GetFilePath(FileType type, const FilePos& pos) {
    return file::GetEpochPath(type, pos.nEpoch) + "/" + file::GetFileName(type, pos.nName);
}

std::string std::to_string(const FilePos& fpos) {
    return strprintf("{ epoch %s, name %s, offset %s }", fpos.nEpoch, fpos.nName, fpos.nOffset);
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
