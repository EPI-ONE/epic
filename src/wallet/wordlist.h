#ifndef __SRC_WALLET_WORDLIST_
#define __SRC_WALLET_WORDLIST_

#include <array>
#include <string>
#include <optional>

using Dict = std::array<std::string, 2048>;

std::optional<Dict> GetWordList(std::string lang);

#endif // __SRC_WALLET_WORDLIST_
