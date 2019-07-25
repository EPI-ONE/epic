
#include "connection.h"
#include "connection_manager.h"
#include "message_header.h"
#include "spdlog.h"

void Connection::Disconnect() {
    if (!valid_) {
        return;
    }
    spdlog::info("[net] Active disconnect: {}", remote_);
    evutil_closesocket(bufferevent_getfd(bev_));
    Release();
}

void Connection::SendMessage(unique_message_t&& message) {
    if (!valid_) {
        return;
    }
    cmptr_->WriteOneMessage_(connection_, message);
}

void Connection::Release() {
    if (!valid_) {
        return;
    }
    valid_ = false;
    cmptr_->DecreaseNum(inbound_);
    connection_ = nullptr;
}
