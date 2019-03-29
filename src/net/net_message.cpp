#include "net_message.h"

void* NetMessage::getConnectionHandle() {
    return connection_handle_;
}

bool NetMessage::VerifyChecksum() {

    if (payload.empty()) {
        return true;
    }

    return getCrc32(payload.data(), payload.size()) == header.checksum;
}
