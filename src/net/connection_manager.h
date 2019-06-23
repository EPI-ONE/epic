#ifndef EPIC_CONNECTION_MANAGER_H
#define EPIC_CONNECTION_MANAGER_H

#include <atomic>
#include <cstdint>
#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <event2/event.h>
#include <event2/listener.h>
#include <functional>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <thread>
#include <tuple>
#include <unordered_map>

#include "blocking_queue.h"
#include "net_message.h"
#include "stream.h"

typedef std::function<void(void* connection_handle, std::string& address, bool inbound)> new_connection_callback_t;
typedef std::function<void(void* connection_handle)> delete_connection_callback_t;

typedef struct bufferevent bufferevent_t;
typedef struct event_base event_base_t;
typedef struct evconnlistener evconnlistener_t;
typedef struct evbuffer evbuffer_t;

/*
 * bool if true means inbound, if false means outbound
 * size_t is the length of next receive message
 */
typedef std::tuple<bool, size_t> bev_info_t;

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

    /*
     * disconnect
     * @param connection_handle
     */
    void Disconnect(const void* connection_handle);
    void Start();
    void Stop();

    /*
     * set the callback function when a new socket accepted or connected
     * @param callback_func
     */
    void RegisterNewConnectionCallback(new_connection_callback_t&& callback_func);

    /*
     * set the callback function when a socket disconnected by remote
     * @param callback_func
     */
    void RegisterDeleteConnectionCallBack(delete_connection_callback_t&& callback_func);

    /*
     * when the send message queue is full ,the function will block the thread
     * @param message
     */
    void SendMessage(NetMessage& message);

    /*
     * when the receive message queue is empty, the function will block the thread
     * @param message
     * @return true if successful, false when an empty queue quit wait
     */
    bool ReceiveMessage(NetMessage& message);

    /*
     * the internal accept or event callback function called by bufferevent
     * @param connection_handle buffervent handle
     * @param address string format ip:port
     * @param inbound true if in connection, false if out connection
     */
    void NewConnectionCallback(void* connection_handle, std::string& address, bool inbound);

    /*
     * the internal event callback function called by bufferevent
     * @param connection_handle
     */
    void DeleteConnectionCallback(void* connection_handle);

    /*
     * the internal read callback function called by bufferevent
     * @param bev
     */
    void ReadMessages(bufferevent_t* bev);

    /*
     * create a bufferevent and insert into the bufferevent map
     * @param base event base
     * @param fd socket fd
     * @param options
     * @return bufferevent pointer
     */
    bufferevent_t* CreateBufferevent(event_base_t* base, evutil_socket_t fd, int options, bool inbound);

    /*
     * free the bufferevent memory and erase from the bufferevent map
     * @param bev
     */
    void FreeBufferevent(bufferevent_t* bev);

    uint32_t GetInboundNum() const;

    uint32_t GetOutboundNum() const;

    uint32_t GetConnectionNum() const;

private:
    event_base_t* base_                                      = nullptr;
    evconnlistener_t* listener_                              = nullptr;
    new_connection_callback_t new_connection_callback_       = nullptr;
    delete_connection_callback_t delete_connection_callback_ = nullptr;

    std::atomic_bool interrupt_send_message_ = false;

    std::thread thread_event_base_;
    std::thread thread_send_message_;

    std::shared_mutex bev_lock_;

    /* the key is bufferevent, the value store the custom information of the bufferevent */
    std::unordered_map<bufferevent_t*, bev_info_t> bufferevent_map_;

    BlockingQueue<NetMessage> receive_message_queue_;
    BlockingQueue<NetMessage> send_message_queue_;

    std::atomic_uint32_t inbound_num_  = 0;
    std::atomic_uint32_t outbound_num_ = 0;

    uint32_t bind_ip_ = INADDR_ANY;

    std::atomic_size_t receive_bytes_    = 0;
    std::atomic_size_t receive_packages_ = 0;
    std::atomic_size_t send_bytes_       = 0;
    std::atomic_size_t send_packages_    = 0;

    std::atomic_size_t checksum_error_bytes_    = 0;
    std::atomic_size_t checksum_error_packages_ = 0;

    void FreeAllBufferevent_();
    bool isExist_(bufferevent_t* bev);
    void ThreadSendMessage_();

    /*
     * write message bytes to bufferevent output buffer
     * @param message
     */
    void WriteOneMessage_(NetMessage& message);

    /*
     * read one message from the input buffer, if success then put the message into receive queue
     * @param bev bufferevent
     * @param next_message_length
     * @return true if successful
     */
    bool ReadOneMessage_(bufferevent_t* bev, size_t& next_message_length);

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
};

#endif // EPIC_CONNECTION_MANAGER_H
