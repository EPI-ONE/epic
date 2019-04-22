#ifndef EPIC_NET_MESSAGE_H
#define EPIC_NET_MESSAGE_H

#include <vector>

#include "crc32.h"
#include "message_header.h"

class NetMessage {
private:
    void* connection_handle_;

public:
    message_header_t header;
    std::vector<unsigned char> payload;

    NetMessage() : connection_handle_(nullptr), header({0, 0, 0, 0}) {
    }

    NetMessage(void* handle, message_header_t& message_header, std::vector<unsigned char>& data)
        : connection_handle_(handle), header(message_header), payload(std::move(data)) {
    }

    NetMessage(void* handle, uint32_t message_type, std::vector<unsigned char>& data)
        : connection_handle_(handle), payload(std::move(data)) {
        header.magic_number   = getMagicNumber();
        header.type           = message_type;
        header.payload_length = payload.size();
        header.checksum       = getCrc32(payload.data(), payload.size());
    }

    NetMessage(void* handle, uint32_t message_type) : connection_handle_(handle) {
        header.magic_number   = getMagicNumber();
        header.type           = message_type;
        header.payload_length = 0;
        header.checksum       = 0;
    }

    void* getConnectionHandle();
    bool VerifyChecksum();
};


#endif // EPIC_NET_MESSAGE_H
