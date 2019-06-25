#ifndef __SRC_UTILS_FILE_UTILS_H__
#define __SRC_UTILS_FILE_UTILS_H__

#include <array>
#include <cerrno>
#include <climits>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>

#include "stream.h"

// TODO(Bgmlover) later can try to use c++17 std::filesystem to implement this
bool CheckDirExist(const std::string& dirPath);
bool CheckFileExist(const std::string& filePath);
bool Mkdir_recursive(const std::string& path);

struct FilePos;
class FileReader;
class FileWriter;

namespace std {
std::string to_string(const FilePos&);
std::string to_string(FileReader&);
std::string to_string(FileWriter&);
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

    ADD_SERIALIZE_METHODS;
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(VARINT(nEpoch));
        READWRITE(VARINT(nName));
        READWRITE(VARINT(nOffset));
    }
};

class FileReader {
public:
    // check if dir and file exist; if not, it can't read
    FileReader(file::FileType type, const FilePos& pos) {
        std::string dir = file::GetEpochPath(type, pos.nEpoch);
        if (!CheckDirExist(dir)) {
            throw std::ios_base::failure("Can't open file because path \"./" + dir + "\" doesn't exits");
        }
        filename_ = dir + "/" + file::GetFileName(type, pos.nName);
        ifbuf_.open(filename_, std::ifstream::in | std::ifstream::binary);
        ifbuf_.seekg(pos.nOffset, std::ios::beg);
    }

    FileReader()                  = delete;
    FileReader(const FileReader&) = delete;
    FileReader& operator=(const FileReader&) = delete;
    ~FileReader()                            = default;

    template <typename Stream>
    FileReader& read(size_t size, Stream& s) {
        size_t nSize = s.size();
        s.resize(nSize + size);
        for (size_t i = nSize; i < s.size(); ++i) {
            ifbuf_.read(&s[i], 1);
        }
        return *this;
    }

    template <typename T>
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

    uint32_t Size() {
        auto currentPos = GetOffset();  // record the current offset
        ifbuf_.seekg(0, std::ios::end); // seek to end of file
        auto size = ifbuf_.tellg();     // record offset as size
        ifbuf_.seekg(currentPos);       // restore offset
        return size;
    }

    void Close() {
        ifbuf_.close();
    }

private:
    std::string filename_;
    std::fstream ifbuf_;
};

// writer can write to
class FileWriter {
public:
    // check if dir exists. If not, create one
    FileWriter(file::FileType type, const FilePos& pos) {
        std::string dir = file::GetEpochPath(type, pos.nEpoch);
        if (!CheckDirExist(dir)) {
            Mkdir_recursive(dir);
        }
        std::string filename_ = dir + "/" + file::GetFileName(type, pos.nName);
        ofbuf_.open(filename_, std::ostream::out | std::ofstream::binary | std::ofstream::app);
        if (!ofbuf_.is_open()) {
            throw std::string("file stream is not opened");
        }
        ofbuf_.seekp(pos.nOffset, std::ios::beg);
    }

    FileWriter()                  = delete;
    FileWriter(const FileWriter&) = delete;
    FileWriter& operator=(const FileWriter&) = delete;
    ~FileWriter()                            = default;

    void Flush() {
        ofbuf_.flush();
    }

    void Close() {
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
};

#endif // __SRC_UTILS_FILE_UTILS_H__
