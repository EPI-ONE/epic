// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef EPIC_CRC32_H
#define EPIC_CRC32_H

#include <cstddef>
#include <cstdint>

uint32_t crc32c(uint8_t* buf, std::size_t length, uint32_t crc = -1);

#endif // EPIC_CRC32_H
