#ifndef EPIC_CONNECTION_MANAGER_H
#define EPIC_CONNECTION_MANAGER_H

#include <atomic>
#include <cstdint>
#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <event2/event.h>
#include <event2/listener.h>
#include <functional>
#include <string>
#include <thread>

#include "blocking_queue.h"
#include "connection.h"
#include "net_message.h"
#include "threadpool.h"

typedef std::pair<shared_connection_t, unique_message_t> connection_message_t;
typedef std::function<void(shared_connection_t&)> connection_callback_t;

typedef struct event_base event_base_t;
typedef struct evconnlistener evconnlistener_t;
typedef struct evbuffer evbuffer_t;


class ConnectionManager {
public:
    ConnectionManager();
    virtual ~ConnectionManager();

    /**
     * bind
     * @param ip ipv4 for example 0x7f000001 means 127.0.0.1
     */
    bool Bind(uint32_t ip);

    /**
     * listen
     * @param port
     * @return true if success
     */
    bool Listen(uint16_t port);

    /**
     * connect
     * @param ip ipv4 for example 0x7f000001 means 127.0.0.1
     * @param port
     * @return true if success
     */
    bool Connect(uint32_t ip, uint16_t port);

    void Start();
    void Stop();

    /*
     * set the callback function when a new socket accepted or connected
     * @param callback_func
     */
    void RegisterNewConnectionCallback(connection_callback_t&& callback_func) {
        new_connection_callback_ = std::move(callback_func);
    }

    /*
     * set the callback function when a socket disconnected by remote
     * @param callback_func
     */
    void RegisterDeleteConnectionCallBack(connection_callback_t&& callback_func) {
        delete_connection_callback_ = std::move(callback_func);
    }

    /*
     * when the receive message queue is empty, the function will block the thread
     * @param message
     * @return true if successful, false when an empty queue quit wait
     */
    bool ReceiveMessage(connection_message_t& message);

    /*
     * the internal accept or event callback function called by bufferevent
     * @param connection_handle buffervent handle
     * @param address string format ip:port
     * @param inbound true if in connection, false if out connection
     */
    void NewConnectionCallback(shared_connection_t& handle) {
        if (new_connection_callback_ != nullptr) {
            new_connection_callback_(handle);
        }
    }

    /*
     * the internal event callback function called by bufferevent
     * @param connection_handle
     */
    void DeleteConnectionCallback(shared_connection_t& handle) {
        if (delete_connection_callback_ != nullptr) {
            delete_connection_callback_(handle);
        }
    }

    /*
     * the internal read callback function called by bufferevent
     * @param bev
     */
    void ReadMessages(bufferevent_t* bev, Connection* handle);

    void IncreaseNum(bool inbound) {
        inbound ? inbound_num_++ : outbound_num_++;
    }

    void DecreaseNum(bool inbound) {
        inbound ? inbound_num_-- : outbound_num_--;
    }

    uint32_t GetInboundNum() const;

    uint32_t GetOutboundNum() const;

    uint32_t GetConnectionNum() const;

    void QuitQueue();

private:
    event_base_t* base_                               = nullptr;
    evconnlistener_t* listener_                       = nullptr;
    connection_callback_t new_connection_callback_    = nullptr;
    connection_callback_t delete_connection_callback_ = nullptr;

    std::thread thread_event_base_;

    BlockingQueue<connection_message_t> receive_message_queue_;

    std::atomic_uint32_t inbound_num_  = 0;
    std::atomic_uint32_t outbound_num_ = 0;

    uint32_t bind_ip_ = INADDR_ANY;

    std::atomic_size_t receive_bytes_    = 0;
    std::atomic_size_t receive_packages_ = 0;
    std::atomic_size_t send_bytes_       = 0;
    std::atomic_size_t send_packages_    = 0;

    std::atomic_size_t checksum_error_bytes_    = 0;
    std::atomic_size_t checksum_error_packages_ = 0;

    ThreadPool serialize_pool_;
    uint32_t serialize_pool_size_ = 4;

    ThreadPool deserialize_pool_;
    uint32_t deserialize_pool_size_ = 2;

    void WriteOneMessage_(shared_connection_t connection, unique_message_t& message);

    /*
     * read one message from the input buffer, if success then put the message into receive queue
     * @param bev bufferevent
     * @param next_message_length
     * @return true if successful
     */
    bool ReadOneMessage_(bufferevent_t* bev, Connection* handle);

    /*
     * seek next message length in a buffer
     * @param buf evbuffer
     * @return the message length to be received
     */
    size_t SeekNextMessageLength_(evbuffer_t* buf);

    /*
     * seek magic number in a buffer
     * @param buf evbuffer
     * @return true if found and release the buffer before the magic number
     */
    bool SeekMagicNumber_(evbuffer_t* buf);

    /*
     * seek the payload length in a buffer which start with magic number
     * @param buf
     * @return the length of the message payload
     */
    size_t SeekMessagePayloadLength_(evbuffer_t* buf);

    friend class Connection;
};

#endif // EPIC_CONNECTION_MANAGER_H
