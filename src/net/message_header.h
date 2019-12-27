// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef EPIC_MESSAGE_HEADER_H
#define EPIC_MESSAGE_HEADER_H

#include <cstdint>

#define MESSAGE_MAGIC_NUMBER_LENGTH 4
#define MESSAGE_COMMAND_LENGTH 4
#define MESSAGE_LENGTH_LENGTH 4
#define MESSAGE_CHECKSUM_LENGTH 4

#define MESSAGE_HEADER_LENGTH \
    MESSAGE_MAGIC_NUMBER_LENGTH + MESSAGE_COMMAND_LENGTH + MESSAGE_LENGTH_LENGTH + MESSAGE_CHECKSUM_LENGTH

#define MAX_MESSAGE_LENGTH 100 * 1024 * 1024

typedef struct {
    uint32_t magic;
    uint8_t type;
    uint8_t countDown;
    uint16_t reserved;
    uint32_t length;
    uint32_t checksum;
} message_header_t;

inline bool VerifyChecksum(const message_header_t& header) {
    return header.checksum == header.magic + header.type + header.countDown + header.length;
}

#endif // EPIC_MESSAGE_HEADER_H
