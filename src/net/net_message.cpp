#include "net_message.h"

const void* NetMessage::GetConnectionHandle() {
    return connection_handle_;
}

bool NetMessage::VerifyChecksum() {
    if (payload.empty()) {
        return true;
    }

    return getCrc32((unsigned char*) payload.data(), payload.size()) == header.checksum;
}
