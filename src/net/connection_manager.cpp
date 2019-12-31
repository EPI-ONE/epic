// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "connection_manager.h"
#include "crc32.h"
#include "message_header.h"
#include "params.h"
#include "spdlog.h"

#include <arpa/inet.h>
#include <event2/buffer.h>
#include <event2/listener.h>
#include <event2/thread.h>

typedef struct sockaddr sockaddr_t;
typedef struct sockaddr_in sockaddr_in_t;

static void MakeSockaddr(sockaddr_t* sa, uint32_t ipv4, uint16_t port) {
    memset(sa, 0, sizeof(sockaddr_t));
    auto sin             = (sockaddr_in_t*) sa;
    sin->sin_family      = AF_INET;
    sin->sin_addr.s_addr = htonl(ipv4);
    sin->sin_port        = htons(port);
}

static int NewSocket(uint32_t ip) {
    /* create socket*/
    int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd == -1) {
        spdlog::warn("[net] Fail to create socket");
        return -1;
    }

    /* bind local ip*/
    sockaddr_t bind_addr;
    MakeSockaddr(&bind_addr, ip, 0);
    if (bind(fd, &bind_addr, sizeof(bind_addr)) == -1) {
        spdlog::warn("[net] Fail to bind ip {}", inet_ntoa({htonl(ip)}));
        close(fd);
        return -1;
    }

    evutil_make_socket_nonblocking(fd);
    return fd;
}

/**
 * convert socket struct to a address string
 * @param addr sockaddr struct
 * @return string format ip:port
 */
static std::string Address2String(sockaddr_t* addr) {
    char buf[64];
    memset(buf, 0, sizeof(buf));
    auto sin = (sockaddr_in_t*) addr;
    evutil_inet_ntop(sin->sin_family, &sin->sin_addr, buf, sizeof(buf));
    return std::string(buf) + ":" + std::to_string(ntohs(sin->sin_port));
}

static Connection* NewConnectionHandle(bufferevent_t* bev, bool inbound, std::string& remote, void* cmptr) {
    auto handle = new Connection(bev, inbound, remote, (ConnectionManager*) cmptr);
    ((ConnectionManager*) cmptr)->IncreaseNum(inbound);
    return handle;
}

/**
 * read callback called by bufferevent
 * @param bev bufferevent
 * @param ctx custom context used for passing the pointer of the ConnectionManager instance
 */
static void ReadCallback(bufferevent_t* bev, void* ctx) {
    auto handle = (Connection*) ctx;
    handle->GetCmptr()->ReadMessages(bev, handle);
}

/**
 * event callback called by bufferevent
 * @param bev bufferevent
 * @param events event flag
 * @param ctx custom context used for passing the pointer of the ConnectionManager instance
 */
static void EventCallback(bufferevent_t* bev, short events, void* ctx) {
    auto handle        = (Connection*) ctx;
    auto cmptr         = handle->GetCmptr();
    std::string remote = handle->GetRemote();

    /* socket connect success */
    if (events & BEV_EVENT_CONNECTED) {
        bufferevent_enable(bev, EV_READ);
        cmptr->NewConnectionCallback(handle->GetHandlePtr());
        spdlog::info("[net] Connected to peer: {}", remote);
    }

    /* socket error */
    if (events & (BEV_EVENT_EOF | BEV_EVENT_READING | BEV_EVENT_WRITING | BEV_EVENT_ERROR)) {
        if (!handle->IsValid()) {
            return;
        }
        cmptr->DeleteConnectionCallback(handle->GetHandlePtr());
        handle->Release();
        spdlog::info("[net] Socket exception: {} event {:x} error {}", remote, events,
                     evutil_socket_error_to_string(EVUTIL_SOCKET_ERROR()));
    }
}

/**
 * accept callback called by bufferevent
 * @param listener evconnlistener
 * @param fd socket fd
 * @param addr sockaddr
 * @param socklen the length of sockaddr
 * @param ctx custom context used for passing the pointer of the ConnectionManager instance
 */
static void AcceptCallback(evconnlistener_t* listener, evutil_socket_t fd, sockaddr_t* addr, int, void* ctx) {
    std::string address = Address2String(addr);
    spdlog::info("[net] Socket accepted: {}", address);

    /* create bufferevent and set callback */
    event_base_t* base = evconnlistener_get_base(listener);
    bufferevent_t* bev = bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE | BEV_OPT_THREADSAFE);
    auto handle        = NewConnectionHandle(bev, true, address, ctx);
    ((ConnectionManager*) ctx)->NewConnectionCallback(handle->GetHandlePtr());
    bufferevent_setcb(bev, ReadCallback, nullptr, EventCallback, handle);
    bufferevent_enable(bev, EV_READ);
}

ConnectionManager::ConnectionManager() {
    evthread_use_pthreads();
    base_ = event_base_new();
}

ConnectionManager::~ConnectionManager() {
    /* free all the memory created by connection manager */
    if (listener_) {
        evconnlistener_free(listener_);
    }

    if (base_) {
        event_base_free(base_);
    }
}

bool ConnectionManager::Bind(uint32_t ip) {
    int fd = NewSocket(ip);
    if (fd == -1) {
        return false;
    }
    close(fd);

    bind_ip_ = ip;
    return true;
}

bool ConnectionManager::Listen(uint16_t port) {
    sockaddr_t sock_addr;
    MakeSockaddr(&sock_addr, bind_ip_, port);
    listener_ = evconnlistener_new_bind(base_, AcceptCallback, this,
                                        LEV_OPT_CLOSE_ON_FREE | LEV_OPT_REUSEABLE | LEV_OPT_THREADSAFE, -1, &sock_addr,
                                        sizeof(sock_addr));

    if (!listener_) {
        return false;
    }

    spdlog::info("[net] Start listen on {}", Address2String(&sock_addr));

    return true;
}

bool ConnectionManager::Connect(uint32_t ip, uint16_t port) {
    /* new socket with bind ip*/
    int fd = NewSocket(bind_ip_);
    if (fd == -1) {
        return false;
    }

    sockaddr_t sock_addr;
    MakeSockaddr(&sock_addr, ip, port);
    std::string remote = Address2String(&sock_addr);

    /* create bufferevent and set callback */
    bufferevent_t* bev = bufferevent_socket_new(base_, fd, BEV_OPT_CLOSE_ON_FREE | BEV_OPT_THREADSAFE);
    auto handle        = NewConnectionHandle(bev, false, remote, this);
    bufferevent_setcb(bev, ReadCallback, nullptr, EventCallback, handle);

    if (bufferevent_socket_connect(bev, &sock_addr, sizeof(sock_addr)) != 0) {
        handle->Release();
        spdlog::info("[net] Failed to connect: {}", remote);
        return false;
    }

    spdlog::trace("[net] Trying to connect: {}", remote);
    return true;
}

void ConnectionManager::Start() {
    serialize_pool_.SetThreadSize(serialize_pool_size_);
    serialize_pool_.Start();
    deserialize_pool_.SetThreadSize(deserialize_pool_size_);
    deserialize_pool_.Start();
    /* thread for listen accept event callback and receive message to the queue */
    thread_event_base_ = std::thread(event_base_loop, base_, EVLOOP_NO_EXIT_ON_EMPTY);
    spdlog::info("[net] Connection manager start");
}

void ConnectionManager::QuitQueue() {
    receive_message_queue_.Quit();
}

void ConnectionManager::Stop() {
    QuitQueue();

    if (listener_) {
        evconnlistener_disable(listener_);
    }

    if (base_) {
        event_base_loopexit(base_, nullptr);
    }

    if (thread_event_base_.joinable()) {
        thread_event_base_.join();
    }

    serialize_pool_.Stop();
    deserialize_pool_.Stop();

    spdlog::info("[net] Connection manager stopped.");
}

bool ConnectionManager::ReceiveMessage(connection_message_t& message) {
    return receive_message_queue_.Take(message);
}

void ConnectionManager::WriteOneMessage_(shared_connection_t connection, unique_message_t& message) {
    serialize_pool_.Execute([connection, message = std::move(message), this]() {
        VStream s;
        message->NetSerialize(s);
        if (!s.empty()) {
            s << crc32c((uint8_t*) s.data(), s.size());
        }

        message_header_t header;
        header.magic    = GetParams().magic;
        header.type     = message->GetType();
        header.length   = s.size();
        header.checksum = header.magic + header.type + header.length;

        if (header.length + MESSAGE_HEADER_LENGTH > MAX_MESSAGE_LENGTH) {
            spdlog::info("[net] Ignoring message with length {} exceeds max bytes {}",
                         header.length + MESSAGE_HEADER_LENGTH, MAX_MESSAGE_LENGTH);
            return;
        }

        evbuffer_t* send_buffer = evbuffer_new();

        /* write message header bytes */
        evbuffer_add(send_buffer, &header, sizeof(message_header_t));

        /* write message payload bytes */
        if (header.length != 0) {
            evbuffer_add(send_buffer, s.data(), header.length);
        }

        bufferevent_write_buffer(connection->GetBev(), send_buffer);
        send_bytes_ += header.length;
        send_packages_ += 1;

        evbuffer_free(send_buffer);
    });
}

void ConnectionManager::ReadMessages(bufferevent_t* bev, Connection* handle) {
    while (ReadOneMessage_(bev, handle)) {
    }
}

bool ConnectionManager::SeekMagicNumber_(evbuffer_t* buf) {
    size_t data_length    = evbuffer_get_length(buf);
    uint32_t magic_number = GetParams().magic;

    if (data_length < MESSAGE_MAGIC_NUMBER_LENGTH) {
        return false;
    }

    struct evbuffer_ptr magic_number_pos = evbuffer_search(buf, (char*) &magic_number, sizeof(uint32_t), nullptr);

    /* if not found, remain the last 4 bytes and release the memory else,
     * maybe the 4 bytes include part of magic number, magic number will complete when receive more bytes */
    if (magic_number_pos.pos == -1) {
        evbuffer_drain(buf, data_length - MESSAGE_MAGIC_NUMBER_LENGTH);
        return false;
    }

    /* if found , release the memory before the magic number*/
    evbuffer_drain(buf, magic_number_pos.pos);
    return true;
}

size_t ConnectionManager::SeekMessagePayloadLength_(evbuffer_t* buf) {
    struct evbuffer_ptr message_length_pos;
    evbuffer_ptr_set(buf, &message_length_pos, MESSAGE_MAGIC_NUMBER_LENGTH + MESSAGE_COMMAND_LENGTH, EVBUFFER_PTR_SET);

    uint32_t message_length = 0;
    evbuffer_copyout_from(buf, &message_length_pos, &message_length, sizeof(message_length));
    return message_length;
}

size_t ConnectionManager::SeekNextMessageLength_(evbuffer_t* buf) {
    /* magic number is not found, wait more bytes*/
    if (!SeekMagicNumber_(buf)) {
        return 0;
    }

    size_t data_length = evbuffer_get_length(buf);

    /* the bytes remain in buffer less than the length of the message header, wait more bytes */
    if (data_length < MESSAGE_HEADER_LENGTH) {
        return 0;
    }

    message_header_t header;
    evbuffer_copyout(buf, &header, MESSAGE_HEADER_LENGTH);
    if (!VerifyChecksum(header)) {
        checksum_error_bytes_ += header.length;
        checksum_error_packages_ += 1;
        return 0;
    }

    /* message header complete , return total message length */
    return SeekMessagePayloadLength_(buf) + MESSAGE_HEADER_LENGTH;
}

bool ConnectionManager::ReadOneMessage_(bufferevent_t* bev, Connection* handle) {
    evbuffer_t* input_buffer = bufferevent_get_input(bev);
    size_t& read_length      = handle->GetLength();

    /* find a new message header and length*/
    if (read_length == 0) {
        read_length = SeekNextMessageLength_(input_buffer);
    }

    /* found a complete message header, but the length is greater than the max limit, discard the message header */
    if (read_length > MAX_MESSAGE_LENGTH) {
        evbuffer_drain(input_buffer, MESSAGE_HEADER_LENGTH);
        read_length = 0;
        return false;
    }

    /* found a complete message header */
    if (read_length > 0) {
        size_t receive_length = evbuffer_get_length(input_buffer);

        /* if the buffer received enough bytes, construct the message instance and put it into the receive queue,
         * otherwise wait more bytes*/
        if (receive_length >= read_length) {
            message_header_t header;
            bufferevent_read(bev, &header, sizeof(header));

            auto payload   = std::make_unique<VStream>();
            uint32_t crc32 = 0;
            if (header.length > sizeof(crc32)) {
                payload->resize(header.length - sizeof(crc32));
                bufferevent_read(bev, payload->data(), header.length - sizeof(crc32));
                bufferevent_read(bev, &crc32, sizeof(crc32));
            }

            deserialize_pool_.Execute(
                [header, payload = std::move(payload), crc32, handle = handle->GetHandlePtr(), this]() {
                    if (header.length == 0 || crc32c((uint8_t*) payload->data(), payload->size()) == crc32) {
                        receive_bytes_ += header.length + MESSAGE_HEADER_LENGTH;
                        receive_packages_ += 1;
                        unique_message_t message = NetMessage::MessageFactory(header.type, *payload);
                        if (message->GetType() != NetMessage::NONE) {
                            receive_message_queue_.Put(std::make_pair(handle, std::move(message)));
                        }
                    } else {
                        checksum_error_bytes_ += header.length;
                        checksum_error_packages_ += 1;
                    }
                });


            read_length = 0;
            return true;
        }
    }

    return false;
}

uint32_t ConnectionManager::GetInboundNum() const {
    return inbound_num_;
}

uint32_t ConnectionManager::GetOutboundNum() const {
    return outbound_num_;
}

uint32_t ConnectionManager::GetConnectionNum() const {
    return inbound_num_ + outbound_num_;
}
