#ifndef __SRC_UTILS_FILE_UTILS_H__
#define __SRC_UTILS_FILE_UTILS_H__

#include "serialize.h"

#include <array>
#include <cerrno>
#include <climits>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>

// TODO(Bgmlover) later can try to use c++17 std::filesystem to implement this
bool CheckDirExist(const std::string& dirPath);
bool CheckFileExist(const std::string& filePath);
bool Mkdir_recursive(const std::string& path);

struct FilePos;
class FileReader;
class FileWriter;

namespace std {
std::string to_string(FileReader& freader);
std::string to_string(FileWriter& fwriter);
} // namespace std

namespace file {
enum FileType : uint8_t { BLK = 0, REC = 1 };
static const std::array<std::string, 2> prefixes{"data/blk/", "data/rec/"};
static const std::array<std::string, 2> typestr{"blk", "rec"};
std::string GetPath(FileType type, uint32_t epoch);
std::string GetFileName(FileType type, uint32_t name);
} // namespace file

struct FilePos {
    uint32_t nEpoch;
    uint32_t nName;
    uint32_t nOffset;

    FilePos() : nEpoch(-1), nName(0), nOffset(0) {}
    FilePos(uint32_t epoch, uint32_t name, uint32_t offset) : nEpoch(epoch), nName(name), nOffset(offset) {}

    bool operator==(const FilePos& another) {
        return (nEpoch == another.nEpoch) && nName == another.nName && nOffset == another.nOffset;
    }

    bool operator!=(const FilePos& another) {
        return !(*this == another);
    }
};

class FileReader {
public:
    // check if dir and file exist; if not, it can't read
    FileReader(file::FileType type, const FilePos& pos) {
        std::string dir = file::GetPath(type, pos.nEpoch);
        if (!CheckDirExist(dir)) {
            throw std::ios_base::failure("Can't open file because it doesn't exits");
        }
        filename_ = dir + "/" + file::GetFileName(type, pos.nName);
        ifbuf_.open(filename_, std::ifstream::in | std::ifstream::binary);
        ifbuf_.seekg(pos.nOffset, std::ios::beg);
    }

    FileReader()                  = delete;
    FileReader(const FileReader&) = delete;
    FileReader& operator=(const FileReader&) = delete;

    ~FileReader() {
        ifbuf_.close();
    }

    template<typename T>
    FileReader& operator>>(T&& obj) {
        ::Deserialize(ifbuf_, obj);
        return *this;
    }

    std::string GetFileName() const {
        return filename_;
    }

    uint32_t GetOffset() {
        return ifbuf_.tellg();
    }

private:
    std::string filename_;
    std::fstream ifbuf_;

    std::string to_string(FileReader& freader);
};

// writer can write to
class FileWriter {
public:
    // check if dir exists. If not, create one
    FileWriter(file::FileType type, const FilePos& pos) {
        std::string dir = file::GetPath(type, pos.nEpoch);
        if (!CheckDirExist(dir)) {
            Mkdir_recursive(dir);
        }
        std::string filename_ = dir + "/" + file::GetFileName(type, pos.nName);
        ofbuf_.open(filename_, std::ostream::out | std::ofstream::binary);
        if (!ofbuf_.is_open()) {
            throw std::string("file stream is not opened");
        }
        ofbuf_.seekp(pos.nOffset, std::ios::beg);
    }

    FileWriter()                  = delete;
    FileWriter(const FileWriter&) = delete;
    FileWriter& operator=(const FileWriter&) = delete;

    ~FileWriter() {
        ofbuf_.close(); 
    }

    template <typename T>
    FileWriter& operator<<(const T& obj) {
        ::Serialize(ofbuf_, obj);
        return *this;
    }

    std::string GetFileName() const {
        return filename_;
    }

    uint32_t GetOffset() {
        return ofbuf_.tellp();
    }

private:
    std::string filename_;
    std::fstream ofbuf_;

    std::string to_string(FileWriter& fwriter);
};

#endif // __SRC_UTILS_FILE_UTILS_H__
