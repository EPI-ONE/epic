#include <arpa/inet.h>
#include <event2/bufferevent.h>
#include <event2/thread.h>
#include <iostream>
#include <pthread.h>

#include "connection_manager.h"
#include "message_header.h"
#include "spdlog.h"

#define MAKE_SOCKADDR_IN(var, address, port) \
    struct sockaddr_in var;                  \
    memset(&var, 0, sizeof(var));            \
    var.sin_family      = AF_INET;           \
    var.sin_addr.s_addr = (htonl(address));  \
    var.sin_port        = (htons(port));

/**
 * convert socket struct to a address string
 * @param addr sockaddr struct
 * @return string format ip:port
 */
static std::string Address2String(struct sockaddr* addr) {
    char buf[64];
    memset(buf, 0, sizeof(buf));
    struct sockaddr_in* sin = (struct sockaddr_in*) addr;
    evutil_inet_ntop(sin->sin_family, &sin->sin_addr, buf, sizeof(buf));
    return std::string(buf) + ":" + std::to_string(ntohs(sin->sin_port));
}

/**
 * get the remote socket address string from bufferevent
 * @param bev bufferevent pointer
 * @return string format ip:port
 */
static std::string getRemoteAddress(struct bufferevent* bev) {
    evutil_socket_t socket_id = bufferevent_getfd(bev);
    struct sockaddr addr;
    socklen_t len = sizeof(sockaddr);
    getpeername(socket_id, &addr, &len);

    return Address2String(&addr);
}

/**
 * read callback called by bufferevent
 * @param bev bufferevent
 * @param ctx custom context used for passing the pointer of the ConnectionManager instance
 */
static void ReadCallback(struct bufferevent* bev, void* ctx) {
    ((ConnectionManager*) ctx)->ReadMessages(bev);
}

/**
 * event callback called by bufferevent
 * @param bev bufferevent
 * @param events event flag
 * @param ctx custom context used for passing the pointer of the ConnectionManager instance
 */
static void EventCallback(struct bufferevent* bev, short events, void* ctx) {
    /* socket connect success */
    if (events & BEV_EVENT_CONNECTED) {
        std::string remote_address = getRemoteAddress(bev);
        spdlog::info("[net] Socket connected: {}", remote_address);
        bufferevent_enable(bev, EV_READ);
        ((ConnectionManager*) ctx)->NewConnectionCallback((void*) bev, remote_address, false);
    }


    /* socket unrecoverable error. for example failed to connect to remote */
    if (events & BEV_EVENT_ERROR) {
        spdlog::info("[net] Socket error: {}", evutil_socket_error_to_string(EVUTIL_SOCKET_ERROR()));
        ((ConnectionManager*) ctx)->FreeBufferevent(bev);
    }

    /* remote disconnect the socket or socket read and write error */
    if (events & (BEV_EVENT_EOF | BEV_EVENT_READING | BEV_EVENT_WRITING)) {
        spdlog::info("[net] Socket disconnected: {}", getRemoteAddress(bev));
        ((ConnectionManager*) ctx)->DeleteConnectionCallback((void*) bev);
        ((ConnectionManager*) ctx)->FreeBufferevent(bev);
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
static void AcceptCallback(struct evconnlistener* listener,
    evutil_socket_t fd,
    struct sockaddr* addr,
    int socklen,
    void* ctx) {
    std::string address = Address2String(addr);
    spdlog::info("[net] Socket accepted: {}", address);

    /* create bufferevent and set callback */
    struct event_base* base = evconnlistener_get_base(listener);
    struct bufferevent* bev =
        ((ConnectionManager*) ctx)->CreateBufferevent(base, fd, BEV_OPT_CLOSE_ON_FREE | BEV_OPT_THREADSAFE);
    bufferevent_setcb(bev, ReadCallback, nullptr, EventCallback, ctx);
    bufferevent_enable(bev, EV_READ);

    ((ConnectionManager*) ctx)->NewConnectionCallback((void*) bev, address, true);
}


ConnectionManager::ConnectionManager() {
    evthread_use_pthreads();
    base                    = event_base_new();
    interrupt_send_message_ = false;
}

ConnectionManager::~ConnectionManager() {
    /* free all the memory created by connection manager */
    if (listener) {
        evconnlistener_free(listener);
    }

    FreeAllBufferevent_();

    if (base) {
        event_base_free(base);
    }
}

int ConnectionManager::Listen(uint32_t port, uint32_t local_bind_address) {
    if (port > 65535) {
        return -1;
    }

    MAKE_SOCKADDR_IN(sock_addr_in, local_bind_address, port);
    listener = evconnlistener_new_bind(base, AcceptCallback, this, LEV_OPT_CLOSE_ON_FREE | LEV_OPT_REUSEABLE, -1,
        (struct sockaddr*) &sock_addr_in, sizeof(sock_addr_in));

    if (!listener) {
        return -1;
    }

    spdlog::info("[net] Start listen on {}", Address2String((struct sockaddr*) &sock_addr_in));

    return 0;
}

int ConnectionManager::Connect(uint32_t remote, uint32_t port) {
    if (port > 65535) {
        return -1;
    }

    /* create bufferevent and set callback */
    struct bufferevent* bev;
    bev = CreateBufferevent(base, -1, BEV_OPT_CLOSE_ON_FREE | BEV_OPT_THREADSAFE);
    bufferevent_setcb(bev, ReadCallback, nullptr, EventCallback, this);

    MAKE_SOCKADDR_IN(sock_addr_in, remote, port);

    if (bufferevent_socket_connect(bev, (struct sockaddr*) &sock_addr_in, sizeof(sock_addr_in)) != 0) {
        spdlog::info("[net] Fail to connect: {}", Address2String((struct sockaddr*) &sock_addr_in));
        FreeBufferevent(bev);
        return -1;
    }

    spdlog::info("[net] Try to connect: {}", Address2String((struct sockaddr*) &sock_addr_in));

    return 0;
}

void ConnectionManager::Disconnect(void* connection_handle) {
    if (isExist_((bufferevent*) connection_handle)) {
        FreeBufferevent((bufferevent*) connection_handle);
        spdlog::info("[net] Active disconnect: {}", getRemoteAddress((struct bufferevent*) connection_handle));
    } else {
        spdlog::info("[net] Not found connection handle {}", connection_handle);
    }
}

void ConnectionManager::Start() {
    /* thread for listen accept event callback and receive message to the queue */
    thread_event_base = std::thread(event_base_loop, base, EVLOOP_NO_EXIT_ON_EMPTY);

    /* thread for send message from the queue */
    thread_send_message = std::thread(&ConnectionManager::ThreadSendMessage_, this);
    spdlog::info("[net] Connection manager start");
}

void ConnectionManager::Stop() {
    interrupt_send_message_ = true;

    if (base) {
        event_base_loopexit(base, nullptr);
    }

    if (thread_event_base.joinable()) {
        thread_event_base.join();
    }

    send_message_queue.Quit();
    receive_message_queue.Quit();

    if (thread_send_message.joinable()) {
        thread_send_message.join();
    }

    spdlog::info("[net] Connection manager stop");
}


void ConnectionManager::RegisterNewConnectionCallback(new_connection_callback_t&& callback_func) {
    new_connection_callback = std::move(callback_func);
}

void ConnectionManager::NewConnectionCallback(void* connection_handle, std::string& address, bool inbound) {
    if (new_connection_callback != nullptr) {
        new_connection_callback(connection_handle, address, inbound);
    }
}

void ConnectionManager::RegisterDeleteConnectionCallBack(delete_connection_callback_t&& callback_func) {
    delete_connection_callback = std::move(callback_func);
}

void ConnectionManager::DeleteConnectionCallback(void* connection_handle) {
    if (delete_connection_callback != nullptr) {
        delete_connection_callback(connection_handle);
    }
}

struct bufferevent* ConnectionManager::CreateBufferevent(struct event_base* base, evutil_socket_t fd, int options) {
    struct bufferevent* bev;
    bev = bufferevent_socket_new(base, fd, options);
    bufferevent_map.insert(std::pair<struct bufferevent*, uint32_t>(bev, 0));
    return bev;
}

void ConnectionManager::FreeBufferevent(struct bufferevent* bev) {
    bufferevent_map.erase(bev);
    bufferevent_free(bev);
}

void ConnectionManager::FreeAllBufferevent_() {
    while (!bufferevent_map.empty()) {
        FreeBufferevent(bufferevent_map.begin()->first);
    }
}

bool ConnectionManager::isExist_(struct bufferevent* bev) {
    bufferevent_map_iter_t iter;
    iter = bufferevent_map.find(bev);
    if (iter != bufferevent_map.end()) {
        return true;
    }
    return false;
}


void ConnectionManager::SendMessage(NetMessage& message) {
    send_message_queue.Put(message);
}

bool ConnectionManager::ReceiveMessage(NetMessage& message) {
    return receive_message_queue.Take(message);
}

void ConnectionManager::ThreadSendMessage_() {
    while (!interrupt_send_message_) {
        NetMessage message;
        if (send_message_queue.Take(message)) {
            WriteOneMessage_(message);
        }
    }
}

void ConnectionManager::WriteOneMessage_(NetMessage& message) {
    bufferevent* bev = (bufferevent*) message.getConnectionHandle();
    if (!isExist_(bev)) {
        return;
    }

    struct evbuffer* send_buffer = evbuffer_new();

    /* write message header bytes */
    evbuffer_add(send_buffer, &message.header, sizeof(message_header_t));

    /* write message payload bytes */
    if (message.header.payload_length != 0) {
        evbuffer_add(send_buffer, message.payload.data(), message.header.payload_length);
    }

    bufferevent_write_buffer(bev, send_buffer);
    evbuffer_free(send_buffer);
}

void ConnectionManager::ReadMessages(struct bufferevent* bev) {
    while (ReadOneMessage_(bev, bufferevent_map.at(bev))) {
    }
}


bool ConnectionManager::SeekMagicNumber_(struct evbuffer* buf) {
    size_t data_length    = evbuffer_get_length(buf);
    uint32_t magic_number = getMagicNumber();

    if (data_length < MESSAGE_MAGIC_NUMBER_LENGTH) {
        return false;
    }

    struct evbuffer_ptr magic_number_pos;
    magic_number_pos = evbuffer_search(buf, (char*) &magic_number, sizeof(uint32_t), nullptr);

    /* not found, remain the last 4 bytes and release the else memory ,
     * maybe the 4 bytes include part of magic number, magic number will complete when receive more bytes */
    if (magic_number_pos.pos == -1) {
        evbuffer_drain(buf, data_length - MESSAGE_MAGIC_NUMBER_LENGTH);
        return false;
    }

    /* found , release the memory before the magic number*/
    evbuffer_drain(buf, magic_number_pos.pos);
    return true;
}

size_t ConnectionManager::SeekMessagePayloadLength_(struct evbuffer* buf) {
    struct evbuffer_ptr message_length_pos;
    evbuffer_ptr_set(buf, &message_length_pos, MESSAGE_MAGIC_NUMBER_LENGTH + MESSAGE_COMMAND_LENGTH, EVBUFFER_PTR_SET);

    uint32_t message_length = 0;
    evbuffer_copyout_from(buf, &message_length_pos, &message_length, sizeof(message_length));
    return message_length;
}

size_t ConnectionManager::SeekNextMessageLength_(struct evbuffer* buf) {
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

bool ConnectionManager::ReadOneMessage_(struct bufferevent* bev, size_t& next_message_length) {
    struct evbuffer* input_buffer = bufferevent_get_input(bev);

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

            std::vector<unsigned char> payload(header.payload_length);
            bufferevent_read(bev, payload.data(), header.payload_length);

            NetMessage message((void*) bev, header, payload);

            /* check message checksum ,discard the message if failed*/
            if (message.VerifyChecksum()) {
                receive_message_queue.Put(message);
            }

            next_message_length = 0;
            return true;
        }
    }

    return false;
}
