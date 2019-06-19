#include <arpa/inet.h>
#include <event2/bufferevent.h>
#include <event2/thread.h>
#include <iostream>
#include <pthread.h>

#include "connection_manager.h"
#include "message_header.h"
#include "spdlog.h"

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

/**
 * get the remote socket address string from bufferevent
 * @param bev bufferevent pointer
 * @return string format ip:port
 */
static std::string getRemoteAddress(bufferevent_t* bev) {
    evutil_socket_t socket_fd = bufferevent_getfd(bev);
    sockaddr_t addr;
    socklen_t len = sizeof(sockaddr);
    int ret       = getpeername(socket_fd, &addr, &len);
    if (ret != 0) {
        return std::string(evutil_socket_error_to_string(EVUTIL_SOCKET_ERROR()));
    } else {
        return Address2String(&addr);
    }
}

/**
 * read callback called by bufferevent
 * @param bev bufferevent
 * @param ctx custom context used for passing the pointer of the ConnectionManager instance
 */
static void ReadCallback(bufferevent_t* bev, void* ctx) {
    ((ConnectionManager*) ctx)->ReadMessages(bev);
}

/**
 * event callback called by bufferevent
 * @param bev bufferevent
 * @param events event flag
 * @param ctx custom context used for passing the pointer of the ConnectionManager instance
 */
static void EventCallback(bufferevent_t* bev, short events, void* ctx) {
    /* socket connect success */
    if (events & BEV_EVENT_CONNECTED) {
        std::string remote = getRemoteAddress(bev);
        bufferevent_enable(bev, EV_READ);
        ((ConnectionManager*) ctx)->NewConnectionCallback((void*) bev, remote, false);
        spdlog::info("[net] Socket connected: {}", remote);
    }

    /* socket error */
    if (events & (BEV_EVENT_EOF | BEV_EVENT_READING | BEV_EVENT_WRITING | BEV_EVENT_ERROR)) {
        std::string remote = getRemoteAddress(bev);
        ((ConnectionManager*) ctx)->DeleteConnectionCallback((void*) bev);
        std::thread t(&ConnectionManager::FreeBufferevent, (ConnectionManager*) ctx, bev);
        t.detach();
        spdlog::info("[net] socket exception: {} event {:x} error {}", remote, events,
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
    bufferevent_t* bev =
        ((ConnectionManager*) ctx)->CreateBufferevent(base, fd, BEV_OPT_CLOSE_ON_FREE | BEV_OPT_THREADSAFE, true);
    bufferevent_setcb(bev, ReadCallback, nullptr, EventCallback, ctx);
    bufferevent_enable(bev, EV_READ);

    ((ConnectionManager*) ctx)->NewConnectionCallback((void*) bev, address, true);
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

    FreeAllBufferevent_();

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
    listener_ = evconnlistener_new_bind(
        base_, AcceptCallback, this, LEV_OPT_CLOSE_ON_FREE | LEV_OPT_REUSEABLE, -1, &sock_addr, sizeof(sock_addr));

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

    /* create bufferevent and set callback */
    bufferevent_t* bev;
    bev = CreateBufferevent(base_, fd, BEV_OPT_CLOSE_ON_FREE | BEV_OPT_THREADSAFE, false);
    bufferevent_setcb(bev, ReadCallback, nullptr, EventCallback, this);

    sockaddr_t sock_addr;
    MakeSockaddr(&sock_addr, ip, port);

    if (bufferevent_socket_connect(bev, &sock_addr, sizeof(sock_addr)) != 0) {
        spdlog::info("[net] Fail to connect: {}", Address2String(&sock_addr));
        FreeBufferevent(bev);
        return false;
    }
    spdlog::info("[net] Try to connect: {}", Address2String(&sock_addr));

    return true;
}

void ConnectionManager::Disconnect(const void* connection_handle) {
    if (isExist_((bufferevent_t*) connection_handle)) {
        spdlog::info("[net] Active disconnect: {}", getRemoteAddress((bufferevent_t*) connection_handle));
        FreeBufferevent((bufferevent_t*) connection_handle);
    } else {
        spdlog::info("[net] Not found connection handle {}", connection_handle);
    }
}

void ConnectionManager::Start() {
    /* thread for listen accept event callback and receive message to the queue */
    thread_event_base_ = std::thread(event_base_loop, base_, EVLOOP_NO_EXIT_ON_EMPTY);
    /* thread for send message from the queue */
    thread_send_message_ = std::thread(&ConnectionManager::ThreadSendMessage_, this);
    spdlog::info("[net] Connection manager start");
}

void ConnectionManager::Stop() {
    send_message_queue_.Quit();
    receive_message_queue_.Quit();

    interrupt_send_message_ = true;

    FreeAllBufferevent_();

    if (base_) {
        event_base_loopexit(base_, nullptr);
    }

    if (thread_send_message_.joinable()) {
        thread_send_message_.join();
    }

    if (thread_event_base_.joinable()) {
        thread_event_base_.join();
    }

    spdlog::info("[net] Connection manager stop");
}

void ConnectionManager::RegisterNewConnectionCallback(new_connection_callback_t&& callback_func) {
    new_connection_callback_ = std::move(callback_func);
}

void ConnectionManager::NewConnectionCallback(void* connection_handle, std::string& address, bool inbound) {
    if (new_connection_callback_ != nullptr) {
        new_connection_callback_(connection_handle, address, inbound);
    }
}

void ConnectionManager::RegisterDeleteConnectionCallBack(delete_connection_callback_t&& callback_func) {
    delete_connection_callback_ = std::move(callback_func);
}

void ConnectionManager::DeleteConnectionCallback(void* connection_handle) {
    if (delete_connection_callback_ != nullptr) {
        delete_connection_callback_(connection_handle);
    }
}

bufferevent_t* ConnectionManager::CreateBufferevent(event_base_t* base, evutil_socket_t fd, int options, bool inbound) {
    bufferevent_t* bev;
    bev = bufferevent_socket_new(base, fd, options);
    inbound ? inbound_num_++ : outbound_num_++;
    std::unique_lock<std::shared_mutex> lock(bev_lock_);
    bufferevent_map_.insert(std::pair<bufferevent_t*, bev_info_t>(bev, std::make_tuple(inbound, 0)));
    return bev;
}

void ConnectionManager::FreeBufferevent(bufferevent_t* bev) {
    std::unique_lock<std::shared_mutex> lock(bev_lock_);
    if (isExist_(bev)) {
        std::get<0>(bufferevent_map_.at(bev)) ? inbound_num_-- : outbound_num_--;
        bufferevent_map_.erase(bev);
        bufferevent_free(bev);
    }
}

void ConnectionManager::FreeAllBufferevent_() {
    while (!bufferevent_map_.empty()) {
        FreeBufferevent(bufferevent_map_.begin()->first);
    }
}

bool ConnectionManager::isExist_(bufferevent_t* bev) {
    return bufferevent_map_.find(bev) != bufferevent_map_.end();
}

void ConnectionManager::SendMessage(NetMessage& message) {
    send_message_queue_.Put(message);
}

bool ConnectionManager::ReceiveMessage(NetMessage& message) {
    return receive_message_queue_.Take(message);
}

void ConnectionManager::ThreadSendMessage_() {
    while (!interrupt_send_message_) {
        NetMessage message;
        if (send_message_queue_.Take(message)) {
            WriteOneMessage_(message);
        }
    }
}

void ConnectionManager::WriteOneMessage_(NetMessage& message) {
    evbuffer_t* send_buffer = evbuffer_new();

    /* write message header bytes */
    evbuffer_add(send_buffer, &message.header, sizeof(message_header_t));

    /* write message payload bytes */
    if (message.header.payload_length != 0) {
        evbuffer_add(send_buffer, message.payload.data(), message.header.payload_length);
    }

    std::shared_lock<std::shared_mutex> lock(bev_lock_);
    auto bev = (bufferevent_t*) message.GetConnectionHandle();
    if (isExist_(bev)) {
        send_bytes_ += message.header.payload_length;
        send_packages_ += 1;
        bufferevent_write_buffer(bev, send_buffer);
    }


    evbuffer_free(send_buffer);
}

void ConnectionManager::ReadMessages(bufferevent_t* bev) {
    try {
        while (ReadOneMessage_(bev, std::get<1>(bufferevent_map_.at(bev)))) {
        }
    } catch (std::exception& e) {
        spdlog::warn(e.what());
    }
}

bool ConnectionManager::SeekMagicNumber_(evbuffer_t* buf) {
    size_t data_length    = evbuffer_get_length(buf);
    uint32_t magic_number = GetMagicNumber();

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

    /* message header complete , return total message length */
    return SeekMessagePayloadLength_(buf) + MESSAGE_HEADER_LENGTH;
}

bool ConnectionManager::ReadOneMessage_(bufferevent_t* bev, size_t& next_message_length) {
    evbuffer_t* input_buffer = bufferevent_get_input(bev);

    /* find a new message header and length*/
    if (next_message_length == 0) {
        next_message_length = SeekNextMessageLength_(input_buffer);
    }

    /* found a complete message header, but the length is greater than the max limit, discard the message header */
    if (next_message_length > MAX_MESSAGE_LENGTH) {
        evbuffer_drain(input_buffer, MESSAGE_HEADER_LENGTH);
        return false;
    }

    /* found a complete message header */
    if (next_message_length > 0) {
        size_t receive_length = evbuffer_get_length(input_buffer);

        /* if the buffer received enough bytes, construct the message instance and put it into the receive queue,
         * otherwise wait more bytes*/
        if (receive_length >= next_message_length) {
            message_header_t header;

            bufferevent_read(bev, &header, sizeof(header));

            VStream payload;
            payload.resize(header.payload_length);
            bufferevent_read(bev, payload.data(), header.payload_length);

            NetMessage message((void*) bev, header, payload);

            /* check message checksum ,discard the message if failed*/
            if (message.VerifyChecksum()) {
                receive_bytes_ += header.payload_length;
                receive_packages_ += 1;
                receive_message_queue_.Put(message);
            } else {
                checksum_error_bytes_ += header.payload_length;
                checksum_error_packages_ += 1;
            }

            next_message_length = 0;
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
