#ifndef __SRC_WALLET_MNEMONICS__
#define __SRC_WALLET_MNEMONICS__

#include "secure.h"

#include <array>
#include <optional>
#include <vector>

class CKey;
class uint256;

class WordReader {
public:
    WordReader();
    WordReader(const WordReader&) = delete;
    WordReader(WordReader&&)      = default;
    WordReader& operator=(const WordReader&) = delete;
    WordReader& operator=(WordReader&&) = default;
    std::optional<uint32_t> GetIndex(std::string);
    std::optional<std::string> GetWord(uint32_t);

private:
    std::array<std::string, 2048> wordArr_;
};

// 128 random bits + 4 bits checksum => 12 11-bit words
class Mnemonics {
public:
    Mnemonics() : entropy_(16) {}
    explicit Mnemonics(std::array<std::string, 12> mnemonics);
    bool Generate();
    std::array<std::string, 12> GetMnemonics();
    void PrintToFile(std::string pathstr);
    std::pair<SecureByte, uint256> GetMasterKeyAndSeed();

private:
    const size_t LENGTH    = 12;
    const size_t WORD_SIZE = 11;
    SecureByte entropy_;
    std::array<uint32_t, 12> words_;

    void BitsToWords();
    bool WordsToBits();
};

#endif // __SRC_WALLET_MNEMONICS__
