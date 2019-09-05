// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef EPIC_FILE_UTILS_H
#define EPIC_FILE_UTILS_H

#include "stream.h"

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
void DeleteDir(const std::string& dirpath);

struct FilePos;
class FileReader;
class FileWriter;
class FileModifier;

namespace std {
std::string to_string(const FilePos&);
std::string to_string(FileReader&);
std::string to_string(FileWriter&);
std::string to_string(FileModifier&);
} // namespace std

namespace file {
enum FileType : uint8_t { BLK = 0, REC = 1 };
static std::string prefix = "data/";
void SetDataDirPrefix(std::string strprefix);
static const std::array<std::string, 2> typestr{"BLK", "REC"};
std::string GetEpochPath(FileType type, uint32_t epoch);
std::string GetFileName(FileType type, uint32_t name);
std::string GetFilePath(FileType type, const FilePos&);
} // namespace file

struct FilePos {
    uint32_t nEpoch;
    uint32_t nName;
    uint32_t nOffset;

    FilePos() : nEpoch(-1), nName(0), nOffset(0) {}
    FilePos(uint32_t epoch, uint32_t name, uint32_t offset) : nEpoch(epoch), nName(name), nOffset(offset) {}
    FilePos(VStream& vs) {
        Deserialize(vs);
    }

    bool SameFileAs(const FilePos& another) {
        return nEpoch == another.nEpoch && nName == another.nName;
    }

    friend bool operator==(const FilePos& a, const FilePos& b) {
        return a.nEpoch == b.nEpoch && a.nName == b.nName && a.nOffset == b.nOffset;
    }

    bool operator!=(const FilePos& another) {
        return !(*this == another);
    }

    bool operator<(const FilePos& another) {
        if (nEpoch < another.nEpoch) {
            return true;
        } else if (nName < another.nName) {
            return true;
        } else if (nOffset < another.nOffset) {
            return true;
        }
        return false;
    }

    ADD_SERIALIZE_METHODS
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(VARINT(nEpoch));
        READWRITE(VARINT(nName));
        READWRITE(VARINT(nOffset));
    }
};

class FileBase {
public:
    FileBase()                = default;
    FileBase(const FileBase&) = delete;
    FileBase& operator=(const FileBase&) = delete;
    virtual ~FileBase()                  = default;

    std::string GetFileName() const {
        return filename_;
    }

    virtual uint32_t GetOffset() {
        return fbuf_.tellp();
    }

    void Close() {
        fbuf_.close();
    }

protected:
    std::string filename_;
    std::fstream fbuf_;
};

class FileReader : public FileBase {
public:
    // check if dir and file exist; if not, it can't read
    FileReader(file::FileType type, const FilePos& pos) {
        std::string dir = file::GetEpochPath(type, pos.nEpoch);
        if (!CheckDirExist(dir)) {
            throw std::ios_base::failure("Can't open file because path \"./" + dir + "\" doesn't exits");
        }
        filename_ = dir + "/" + file::GetFileName(type, pos.nName);
        fbuf_.open(filename_, std::ifstream::in | std::ifstream::binary);
        fbuf_.seekg(pos.nOffset, std::ios::beg);
    }
    FileReader() = delete;

    template <typename Stream>
    FileReader& read(size_t size, Stream& s) {
        size_t nSize = s.size();
        s.resize(nSize + size);
        fbuf_.read(&s[nSize], size);
        return *this;
    }

    template <typename T>
    FileReader& operator>>(T&& obj) {
        ::Deserialize(fbuf_, obj);
        return *this;
    }

    uint32_t GetOffset() override {
        return fbuf_.tellg();
    }

    uint32_t Size() {
        auto currentPos = GetOffset(); // record the current offset
        fbuf_.seekg(0, std::ios::end); // seek to end of file
        auto size = fbuf_.tellg();     // record offset as size
        fbuf_.seekg(currentPos);       // restore offset
        return size;
    }
};

class FileWriter : public FileBase {
public:
    // check if dir exists. If not, create one
    FileWriter(file::FileType type, const FilePos& pos) {
        std::string dir = file::GetEpochPath(type, pos.nEpoch);
        if (!CheckDirExist(dir)) {
            Mkdir_recursive(dir);
        }
        std::string filename_ = dir + "/" + file::GetFileName(type, pos.nName);
        fbuf_.open(filename_, std::ios_base::out | std::ios_base::binary | std::ios_base::app);
        if (!fbuf_.is_open()) {
            throw std::string("file writer stream is not opened");
        }
    }
    FileWriter() = delete;

    void Flush() {
        fbuf_.flush();
    }

    template <typename T>
    FileWriter& operator<<(const T& obj) {
        ::Serialize(fbuf_, obj);
        return *this;
    }
};

class FileModifier : public FileBase {
public:
    FileModifier(file::FileType type, const FilePos& pos) {
        std::string filename_ = file::GetFilePath(type, pos);
        if (!CheckFileExist(filename_)) {
            throw std::string("target file does not exists");
        }
        fbuf_.open(filename_, std::ios_base::in | std::ios_base::out | std::ios_base::binary);
        if (!fbuf_.is_open()) {
            throw std::string("file modifier stream is not opened");
        }
        fbuf_.seekp(pos.nOffset, std::ios_base::beg);
    }
    FileModifier() = delete;

    void Flush() {
        fbuf_.flush();
    }

    template <typename T>
    FileModifier& operator<<(const T& obj) {
        ::Serialize(fbuf_, obj);
        return *this;
    }
};

#endif // EPIC_FILE_UTILS_H
