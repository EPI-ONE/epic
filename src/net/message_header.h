
#ifndef EPIC_MESSAGE_HEADER_H
#define EPIC_MESSAGE_HEADER_H

#include <cstdint>

#define MESSAGE_MAGIC_NUMBER_LENGTH 4
#define MESSAGE_COMMAND_LENGTH 4
#define MESSAGE_LENGTH_LENGTH 4
#define MESSAGE_CHECKSUM_LENGTH 4

#define MESSAGE_HEADER_LENGTH MESSAGE_MAGIC_NUMBER_LENGTH+MESSAGE_COMMAND_LENGTH+MESSAGE_LENGTH_LENGTH+MESSAGE_CHECKSUM_LENGTH

#define MAX_MESSAGE_LENGTH 4 * 1000 * 1000

enum protocol_message_type {
    MESSAGE_TYPE_VERSION = 1,
    MESSAGE_TYPE_VERACK
};

typedef struct {
    uint32_t magic_number;
    uint32_t type;
    uint32_t payload_length;
    uint32_t checksum;
} message_header_t;

/* magic number defined by protocol */
inline uint32_t getMagicNumber() {
    return 0xD9B4BEF9;
}

#endif //EPIC_MESSAGE_HEADER_H
