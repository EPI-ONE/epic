// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "file_utils.h"
#include "crc32.h"
#include "spdlog/spdlog.h"
#include "tinyformat.h"

#include <filesystem>
#include <regex>

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
bool MkdirRecursive(const std::string& path) {
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

std::string file::GetFilePath(FileType type, const FilePos pos) {
    return file::GetEpochPath(type, pos.nEpoch) + "/" + file::GetFileName(type, pos.nName);
}

std::string std::to_string(const FilePos& fpos) {
    return strprintf("{ epoch %s, name %s, offset %s }", fpos.nEpoch, fpos.nName, fpos.nOffset);
}

std::string std::to_string(FileReader& freader) {
    std::string s;
    s += strprintf("Reading file %s at position %i.", freader.GetFileName(), freader.GetOffsetG());
    return s;
}

std::string std::to_string(FileWriter& fwriter) {
    std::string s;
    s += strprintf("Writing file %s at position %i.", fwriter.GetFileName(), fwriter.GetOffsetP());
    return s;
}

std::string std::to_string(FileModifier& fmodifier) {
    std::string s;
    s += strprintf("Modifying file %s at position %i.", fmodifier.GetFileName(), fmodifier.GetOffsetP());
    return s;
}

bool DeleteInvalidFile(const std::filesystem::directory_entry& file,
                       FilePos& pos,
                       file::FileType type,
                       std::vector<std::filesystem::directory_entry>& toBeDeleted) {
    std::string filepath = file.path().string();
    std::string filename = file.path().filename();
    auto filename_reg    = type == file::BLK ? std::regex(file::blk_name_regex) : std::regex(file::vtx_name_regex);
    if (!regex_match(filename, filename_reg)) {
        return true;
    }
    // check if it is an invalid file, prune it if part of the file is valid
    uint32_t name = std::stoi(filename.substr(3));
    if (name > pos.nName) {
        toBeDeleted.push_back(file);
        spdlog::debug("Delete invalid file {}", filepath);
    } else if (name == pos.nName) {
        // delete the file if size <= size of checksum
        if (pos.nOffset <= file::checksum_size) {
            toBeDeleted.push_back(file);
            spdlog::debug("Delete invalid file {}", filepath);
        } else if (pos.nOffset < std::filesystem::file_size(file.path())) {
            std::filesystem::resize_file(file.path(), pos.nOffset);
            // update checksum after truncating it
            FilePos pos1(pos.nEpoch, name, 4);
            file::CalculateChecksum(type, pos1);
            spdlog::debug("Truncate file {} and update its checksum", filepath);
        }
    }
    return true;
}

bool DeleteInvalidDir(const std::filesystem::directory_entry& epoch_dir,
                      FilePos& pos,
                      file::FileType type,
                      std::vector<std::filesystem::directory_entry>& toBeDeleted) {
    std::string epoch_dirname = epoch_dir.path().filename();
    std::regex epoch_reg(file::epoch_regex);
    if (!epoch_dir.is_directory() || !regex_match(epoch_dirname, epoch_reg)) {
        return true;
    }

    // check if it is an invalid directory
    uint32_t epoch = std::stoi(epoch_dirname.substr(1));
    if (epoch > pos.nEpoch) {
        toBeDeleted.push_back(epoch_dir);
        spdlog::debug("Delete invalid directory {}", epoch_dirname);
    } else if (epoch == pos.nEpoch) {
        // traverse all BLK/VEX files in the epoch directory
        std::filesystem::directory_iterator fileList(epoch_dir.path().relative_path());
        for (const std::filesystem::directory_entry& file : fileList) {
            DeleteInvalidFile(file, pos, type, toBeDeleted);
        }
    }
    return true;
}


bool file::DeleteInvalidFiles(FilePos& pos, file::FileType type) {
    std::string dir = file::prefix + file::typestr[type];
    std::filesystem::directory_iterator dirList(dir);
    std::vector<std::filesystem::directory_entry> toBeDeleted{};

    // traverse all epoch directories in the dir
    for (const std::filesystem::directory_entry& epoch_dir : dirList) {
        DeleteInvalidDir(epoch_dir, pos, type, toBeDeleted);
    }

    for (auto& file : toBeDeleted) {
        std::filesystem::remove_all(file);
    }

    std::filesystem::directory_iterator newdirList(dir);
    for (const std::filesystem::directory_entry& epoch_dir : newdirList) {
        if (std::filesystem::is_directory(epoch_dir.path()) && std::filesystem::is_empty(epoch_dir.path())) {
            spdlog::debug("Delete empty directory {}", epoch_dir.path().relative_path().string());
            std::filesystem::remove(epoch_dir);
        }
    }
    return true;
}

void file::CalculateChecksum(file::FileType type, FilePos pos) {
    pos.nOffset = file::checksum_size;
    FileModifier modifier(type, pos);
    VStream stream;
    modifier.read(modifier.Size() - file::checksum_size, stream);
    if (stream.empty()) {
        return;
    }
    auto checksum = crc32c((uint8_t*) stream.data(), stream.size());
    modifier.SetOffsetP(0, std::ios_base::beg);
    modifier << checksum;
    modifier.Flush();
    modifier.Close();
}

void file::UpdateChecksum(file::FileType type, FilePos& pos, size_t last_offset) {
    pos.nOffset = 0;
    FileModifier modifier(type, pos);
    VStream stream;
    modifier.read(file::checksum_size, stream);
    uint32_t old_checksum =
        (uint8_t) stream[0] | (uint8_t) stream[1] << 8 | (uint8_t) stream[2] << 16 | (uint8_t) stream[3] << 24;
    stream.clear();
    modifier.SetOffsetP(last_offset, std::ios_base::beg);
    modifier.read(modifier.Size() - last_offset, stream);
    if (stream.empty()) {
        return;
    }
    auto checksum = crc32c((uint8_t*) stream.data(), stream.size(), ~old_checksum);
    modifier.SetOffsetP(0, std::ios_base::beg);
    modifier << checksum;
    modifier.Flush();
    modifier.Close();
}

bool file::ValidateChecksum(file::FileType type, FilePos pos) {
    pos.nOffset = 0;
    FileReader reader(type, pos);
    VStream stream;
    reader.read(reader.Size(), stream);
    reader.Close();
    if (stream.size() == file::checksum_size) {
        return true;
    }
    uint32_t calChecksum = crc32c((uint8_t*) stream.data() + file::checksum_size, stream.size() - file::checksum_size);
    uint32_t checksum =
        (uint8_t) stream[0] | (uint8_t) stream[1] << 8 | (uint8_t) stream[2] << 16 | (uint8_t) stream[3] << 24;
    return calChecksum == checksum;
}

uint64_t file::GetFileSize(file::FileType type, FilePos pos) {
    std::string path = GetFilePath(type, pos);
    std::filesystem::directory_entry file(path);
    if (!file.exists()) {
        return 0;
    }
    return std::filesystem::file_size(file.path());
}

std::unordered_set<uint32_t> file::GetAllEpoch(file::FileType type) {
    std::unordered_set<uint32_t> results;
    std::string dir = file::prefix + file::typestr[type];
    std::filesystem::directory_iterator dirList(dir);

    // traverse all epoch directories in the dir
    for (const std::filesystem::directory_entry& epoch_dir : dirList) {
        std::string epoch_dirname = epoch_dir.path().filename();
        std::regex epoch_reg(file::epoch_regex);
        if (!epoch_dir.is_directory() || !regex_match(epoch_dirname, epoch_reg)) {
            continue;
        }
        // check if it is an invalid directory
        uint32_t epoch = std::stoi(epoch_dirname.substr(1));
        results.insert(epoch);
    }
    return results;
}

std::unordered_set<uint32_t> file::GetAllName(uint32_t epoch, file::FileType type) {
    std::unordered_set<uint32_t> results;
    std::string dir = file::GetEpochPath(type, epoch);
    std::filesystem::directory_iterator fileList(dir);

    for (const std::filesystem::directory_entry& epoch_dir : fileList) {
        std::string filename = epoch_dir.path().filename();
        auto filename_reg    = type == file::BLK ? std::regex(file::blk_name_regex) : std::regex(file::vtx_name_regex);
        if (epoch_dir.is_directory() || !regex_match(filename, filename_reg)) {
            continue;
        }
        uint32_t name = std::stoi(filename.substr(3));
        results.insert(name);
    }
    return results;
}
