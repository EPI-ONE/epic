#ifndef EPIC_NET_MESSAGE_H
#define EPIC_NET_MESSAGE_H

#include <vector>
#include <sstream>

#include "stream.h"
#include "message_header.h"
#include "crc32.h"

class NetMessage {

    private:
        const void *connection_handle_;
    public:
        message_header_t header;
        VStream payload;

        NetMessage() : connection_handle_(nullptr), header({0, 0, 0, 0}) {}

        NetMessage(void *handle, message_header_t &message_header, VStream &data) :
            connection_handle_(handle), header(message_header), payload(std::move(data)) {}

        NetMessage(const void *handle, uint32_t message_type, VStream &&data)
            : connection_handle_(handle), payload(std::move(data)) {
            header.magic_number = getMagicNumber();
            header.type = message_type;
            header.payload_length = payload.size();
            header.checksum = getCrc32((unsigned char *) payload.data(), payload.size());
        }

        NetMessage(const void *handle, uint32_t message_type, VStream &data)
            : connection_handle_(handle), payload(std::move(data)) {
            header.magic_number = getMagicNumber();
            header.type = message_type;
            header.payload_length = payload.size();
            header.checksum = getCrc32((unsigned char *) payload.data(), payload.size());
        }

        NetMessage(void *handle, uint32_t message_type) : connection_handle_(handle) {
            header.magic_number = getMagicNumber();
            header.type = message_type;
            header.payload_length = 0;
            header.checksum = 0;
        }

        const void *getConnectionHandle();
        bool VerifyChecksum();

};

#endif //EPIC_NET_MESSAGE_H
