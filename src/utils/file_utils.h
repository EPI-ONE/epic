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
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <unordered_set>

// TODO(Bgmlover) later can try to use c++17 std::filesystem to implement this
bool CheckDirExist(const std::string& dirPath);
bool CheckFileExist(const std::string& filePath);
bool MkdirRecursive(const std::string& path);
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
enum FileType : uint8_t { BLK = 0, VTX = 1 };
const uint32_t checksum_size     = sizeof(uint32_t);
static std::string prefix        = "data/";
const std::string epoch_regex    = "^E\\d{6}$";
const std::string blk_name_regex = "^BLK\\d{6}.dat$";
const std::string vtx_name_regex = "^VTX\\d{6}.dat$";
void SetDataDirPrefix(std::string strprefix);
static const std::array<std::string, 2> typestr{"BLK", "VTX"};

std::string GetEpochPath(FileType type, uint32_t epoch);
std::string GetFileName(FileType type, uint32_t name);
std::string GetFilePath(FileType type, const FilePos pos);
void CalculateChecksum(file::FileType type, FilePos pos);
void UpdateChecksum(file::FileType type, FilePos& pos, size_t last_offset);
bool ValidateChecksum(file::FileType type, FilePos pos);
bool DeleteInvalidFiles(FilePos& pos, file::FileType type);

uint64_t GetFileSize(file::FileType type, FilePos pos);
std::unordered_set<uint32_t> GetAllEpoch(FileType type);
std::unordered_set<uint32_t> GetAllName(uint32_t epoch, FileType type);
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

namespace std {
template <>
struct hash<FilePos> {
    size_t operator()(const FilePos& f) const {
        return f.nEpoch << 16 | f.nName;
    }
};
} // namespace std

class FileBase {
public:
    //    FileBase() = default;
    FileBase(const std::string& dir, const std::string& filename, std::ios_base::openmode mode, bool create) {
        if (!CheckDirExist(dir)) {
            if (create) {
                MkdirRecursive(dir);
            } else {
                throw std::ios_base::failure("Can't open file because path \"./" + dir + "\" doesn't exits");
            }
        }
        filename_ = dir + '/' + filename;
        fbuf_.open(filename_, mode);
        if (!fbuf_.is_open()) {
            throw std::ios_base::failure("file can't be opened");
        }
    }
    FileBase(const FileBase&) = delete;
    FileBase& operator=(const FileBase&) = delete;
    virtual ~FileBase()                  = default;

    std::string GetFileName() const {
        return filename_;
    }


    void Close() {
        fbuf_.close();
    }

protected:
    std::string filename_;
    std::fstream fbuf_;

    template <typename Stream>
    FileBase& read(size_t size, Stream& s) {
        size_t nSize = s.size();
        s.resize(nSize + size);
        fbuf_.read(&s[nSize], size);
        return *this;
    }

    template <typename T>
    FileBase& operator>>(T&& obj) {
        ::Deserialize(fbuf_, obj);
        return *this;
    }

    template <typename T>
    FileBase& operator<<(const T& obj) {
        ::Serialize(fbuf_, obj);
        return *this;
    }
    uint32_t GetOffsetP() {
        return fbuf_.tellp();
    }

    uint32_t GetOffsetG() {
        return fbuf_.tellg();
    }

    void SetOffsetP(uint32_t offset, std::ios_base::seekdir seekdir) {
        fbuf_.seekp(offset, seekdir);
    }

    uint32_t Size() {
        auto currentPos = GetOffsetG(); // record the current offset
        fbuf_.seekg(0, std::ios::end);  // seek to end of file
        auto size = fbuf_.tellg();      // record offset as size
        fbuf_.seekg(currentPos);        // restore offset
        return size;
    }

    void Flush() {
        fbuf_.flush();
    }
};

class FileReader : public FileBase {
public:
    // check if dir and file exist; if not, it can't read
    FileReader(file::FileType type, const FilePos& pos)
        : FileBase(file::GetEpochPath(type, pos.nEpoch),
                   file::GetFileName(type, pos.nName),
                   std::ifstream::in | std::ifstream::binary,
                   false) {
        fbuf_.seekg(pos.nOffset, std::ios::beg);
    }


    using FileBase::read;
    using FileBase::operator>>;
    using FileBase::GetOffsetG;
    using FileBase::SetOffsetP;
    using FileBase::Size;
};

class FileWriter : public FileBase {
public:
    // check if dir exists. If not, create one
    FileWriter(file::FileType type, const FilePos& pos)
        : FileBase(file::GetEpochPath(type, pos.nEpoch),
                   file::GetFileName(type, pos.nName),
                   std::ios_base::out | std::ios_base::binary | std::ios_base::app,
                   true) {
        fbuf_.seekp(pos.nOffset, std::ios::beg);
    }
    FileWriter() = delete;

    using FileBase::Flush;
    using FileBase::operator<<;
    using FileBase::GetOffsetP;
    using FileBase::SetOffsetP;
    using FileBase::Size;
};

class FileModifier : public FileBase {
public:
    FileModifier(file::FileType type, const FilePos& pos)
        : FileBase(file::GetEpochPath(type, pos.nEpoch),
                   file::GetFileName(type, pos.nName),
                   std::ios_base::in | std::ios_base::out | std::ios_base::binary,
                   false) {
        fbuf_.seekp(pos.nOffset, std::ios_base::beg);
    }
    FileModifier() = delete;

    using FileBase::read;
    using FileBase::Size;
    using FileBase::operator<<;
    using FileBase::Flush;
    using FileBase::GetOffsetG;
    using FileBase::GetOffsetP;
    using FileBase::SetOffsetP;
};

#endif // EPIC_FILE_UTILS_H
