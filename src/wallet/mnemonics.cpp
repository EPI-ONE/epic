// Copyright (c) 2020 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "mnemonics.h"

#include "big_uint.h"
#include "file_utils.h"
#include "hash.h"
#include "key.h"
#include "random.h"
#include "spdlog.h"
#include "utilstrencodings.h"
#include "wordlist.h"

#include <algorithm>
#include <iostream>
#include <iterator>
#include <openssl/evp.h>
#include <string>

#define BYTE_TO_BINARY_PATTERN "%c%c%c%c%c%c%c%c"
#define BYTE_TO_BINARY(byte)                                                                                    \
    (byte & 0x80 ? '1' : '0'), (byte & 0x40 ? '1' : '0'), (byte & 0x20 ? '1' : '0'), (byte & 0x10 ? '1' : '0'), \
        (byte & 0x08 ? '1' : '0'), (byte & 0x04 ? '1' : '0'), (byte & 0x02 ? '1' : '0'), (byte & 0x01 ? '1' : '0')

WordReader::WordReader() {
    std::string lang = "english";

    if (auto words = GetWordList(lang)) {
        wordArr_ = std::move(*words);
    } else {
        spdlog::error("Fail to find desired language.");
    }
}

std::optional<uint32_t> WordReader::GetIndex(std::string word) {
    auto iter = std::lower_bound(wordArr_.begin(), wordArr_.end(), word);
    if (iter != wordArr_.end() && *iter == word) {
        return std::distance(wordArr_.begin(), iter);
    }
    return {};
}

std::optional<std::string> WordReader::GetWord(uint32_t index) {
    if (index < wordArr_.size()) {
        return wordArr_[index];
    }
    return {};
}

bool Mnemonics::Load(std::array<std::string, 12> mnemonics) {
    WordReader reader{};
    for (size_t i = 0; i < mnemonics.size(); i++) {
        if (auto word = reader.GetIndex(mnemonics[i])) {
            words_[i] = *word;
        } else {
            spdlog::info("Word {} is not in word list", mnemonics[i]);
            return false;
        }
    }
    if (!WordsToBits()) {
        spdlog::info("Entropy does not match its check sum");
        return false;
    }
    return true;
}

bool Mnemonics::Generate() {
    bool flag = GetOpenSSLRand(entropy_.data(), entropy_.size());
    if (flag) {
        BitsToWords();
    }
    return flag;
}

std::array<std::string, 12> Mnemonics::GetMnemonics() {
    std::array<std::string, 12> result;
    WordReader reader{};

    for (size_t i = 0; i < result.size(); i++) {
        if (auto ww = reader.GetWord(words_[i])) {
            result[i] = *ww;
        }
    }

    return result;
}

void Mnemonics::PrintToFile(std::string pathstr) {
    auto mne      = GetMnemonics();
    auto filePath = pathstr + "mnemonics.txt";
    std::ofstream writer{filePath, std::ios::out | std::ios::trunc};
    for (auto& word : mne) {
        writer << word << " ";
    }
    writer << std::endl;
    writer.close();
    spdlog::info("save mnemonics to {}", filePath);
}

void Mnemonics::BitsToWords() {
    uint256 checksum = HashSHA2<1>(entropy_.data(), entropy_.size());

    SecureString binaryStr;
    binaryStr.reserve(LENGTH * WORD_SIZE);

    char singleByte[9];
    memset(singleByte, 0, sizeof(singleByte));

    // convert each byte to binary in binary str
    for (auto& byte : entropy_) {
        sprintf(singleByte, BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY(byte));
        binaryStr.append(singleByte, 8);
    }

    sprintf(singleByte, BYTE_TO_BINARY_PATTERN, BYTE_TO_BINARY(*checksum.begin()));
    binaryStr.append(singleByte, 4);
    memset(singleByte, 0, sizeof(singleByte));

    for (size_t i = 0; i < LENGTH; i++) {
        SecureString wordBinary{binaryStr, i * WORD_SIZE, WORD_SIZE};
        uint32_t wordnum = 0;
        // convert string of '1' and '0' to integer
        for (size_t j = 0; j < WORD_SIZE; j++) {
            wordnum += (wordBinary[j] == '1' ? 1 : 0) << (WORD_SIZE - 1 - j);
        }
        words_[i] = wordnum;
    }
}

bool Mnemonics::WordsToBits() {
    std::vector<uint32_t> binarys;
    binarys.reserve(LENGTH * WORD_SIZE);

    for (size_t i = 0; i < LENGTH; i++) {
        for (size_t j = 0; j < WORD_SIZE; j++) {
            binarys.emplace_back(words_[i] & (1 << (WORD_SIZE - 1 - j)) ? 1 : 0);
        }
    }

    for (size_t i = 0; i < 16; i++) {
        entropy_[i] = 0;
        for (size_t j = 0; j < 8; j++) {
            entropy_[i] += binarys[i * 8 + j] << (8 - 1 - j);
        }
    }

    // checking check sum
    uint256 checksum     = HashSHA2<1>(entropy_.data(), entropy_.size());
    unsigned char origin = *checksum.begin() >> 4;
    unsigned char exam   = 0;
    for (size_t i = 0; i < 4; i++) {
        exam += binarys[128 + i] << (4 - 1 - i);
    }

    return (origin == exam);
}

std::pair<SecureByte, uint256> Mnemonics::GetMasterKeyAndSeed() {
    SecureByte out;
    out.resize(64);
    static const unsigned char salt[] = {'e', 'p', 'i', 'c', 'i', 's', 's', 'e', 'c', 'u', 'r', 'e'};
    int ret = PKCS5_PBKDF2_HMAC_SHA1((char*) (entropy_.data()), entropy_.size(), salt, sizeof(salt), 1, 64, out.data());
    if (!ret) {
        return {SecureByte{}, uint256{}};
    }

    SecureByte out1(32), out2(32);
    memcpy(out1.data(), out.data(), out1.size());
    memcpy(out2.data(), out.data() + 32, out2.size());

    uint256 chaincode{out2.data(), out2.size()};
    return {out1, chaincode};
}

CExtKey Mnemonics::GetHDMaster() {
    CExtKey master;
    master.SetSeed(entropy_.data(), entropy_.size());
    return master;
}
