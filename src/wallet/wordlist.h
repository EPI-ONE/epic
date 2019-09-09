// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef EPIC_WORDLIST_H
#define EPIC_WORDLIST_H

#include <array>
#include <string>
#include <optional>

using Dict = std::array<std::string, 2048>;

std::optional<Dict> GetWordList(std::string lang);

#endif // EPIC_WORDLIST_H
