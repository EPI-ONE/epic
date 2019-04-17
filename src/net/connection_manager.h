#ifndef EPIC_CONNECTION_MANAGER_H
#define EPIC_CONNECTION_MANAGER_H


#include <cstdint>
#include <event2/listener.h>
#include <event2/event.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <functional>
#include <string>
#include <thread>
#include <unordered_map>
#include <atomic>

#include "net_message.h"
#include "blocking_queue.h"
#include "stream.h"

typedef std::function<void(void* connection_handle, std::string& address, bool inbound)> new_connection_callback_t;
typedef std::function<void(void* connection_handle)> delete_connection_callback_t;
typedef std::unordered_map<struct bufferevent*, size_t>::iterator bufferevent_map_iter_t;

class ConnectionManager {

private:
    struct event_base *base = nullptr;
    struct evconnlistener *listener = nullptr;
    new_connection_callback_t new_connection_callback = nullptr;
    delete_connection_callback_t delete_connection_callback = nullptr;

    std::atomic<bool> interrupt_send_message_;

    std::thread thread_event_base;
    std::thread thread_send_message;


    /* the key is bufferevent, the value is the length of next receive message */
    std::unordered_map<struct bufferevent*, size_t> bufferevent_map;

    BlockingQueue<NetMessage> receive_message_queue;
    BlockingQueue<NetMessage> send_message_queue;

    void FreeAllBufferevent_();
    bool isExist_(struct bufferevent *bev);
    void ThreadSendMessage_();

    /**
     * write message bytes to bufferevent output buffer
     * @param message
     */
    void WriteOneMessage_(NetMessage &message);

    /**
     * read one message from the input buffer, if success then put the message into receive queue
     * @param bev bufferevent
     * @param next_message_length
     * @return true if successful
     */
    bool ReadOneMessage_(struct bufferevent *bev, size_t &next_message_length);

    /**
     * seek next message length in a buffer
     * @param buf evbuffer
     * @return the message length to be received
     */
    size_t SeekNextMessageLength_(struct evbuffer *buf);

    /**
     * seek magic number in a buffer
     * @param buf evbuffer
     * @return true if found and release the buffer before the magic number
     */
    bool SeekMagicNumber_(struct evbuffer *buf);

    /**
     * seek the payload length in a buffer which start with magic number
     * @param buf
     * @return the length of the message payload
     */
    size_t SeekMessagePayloadLength_(struct evbuffer *buf);


public:
    ConnectionManager();
    virtual ~ConnectionManager();

    /**
     * listen
     * @param port less than 65536
     * @param local_bind_address ipv4 for example 0x7f000001 means 127.0.0.1
     * @return 0 if successful, other on failure.
     */
    int Listen(uint32_t port, uint32_t local_bind_address = 0);

    /**
     * connect
     * @param remote ipv4 for example 0x7f000001 means 127.0.0.1
     * @param port less than 65536
     * @return 0 if successful, other on failure.
     */
    int Connect(uint32_t remote, uint32_t port);

    /**
     * disconnect
     * @param connection_handle
     */
    void Disconnect(void* connection_handle);
    void Start();
    void Stop();

    /**
     * set the callback function when a new socket accepted or connected
     * @param callback_func
     */
    void RegisterNewConnectionCallback(new_connection_callback_t&& callback_func);

    /**
     * set the callback function when a socket disconnected by remote
     * @param callback_func
     */
    void RegisterDeleteConnectionCallBack(delete_connection_callback_t&& callback_func);

    /**
     * when the send message queue is full ,the function will block the thread
     * @param message
     */
    void SendMessage(NetMessage& message);

    /**
     * when the receive message queue is empty, the function will block the thread
     * @param message
     * @return true if successful, false when an empty queue quit wait
     */
    bool ReceiveMessage(NetMessage& message);

    /**
     * the internal accept or event callback function called by bufferevent
     * @param connection_handle buffervent handle
     * @param address string format ip:port
     * @param inbound true if in connection, false if out connection
     */
    void NewConnectionCallback(void* connection_handle, std::string& address, bool inbound);

    /**
     * the internal event callback function called by bufferevent
     * @param connection_handle
     */
    void DeleteConnectionCallback(void* connection_handle);

    /**
     * the internal read callback function called by bufferevent
     * @param bev
     */
    void ReadMessages(struct bufferevent *bev);

    /**
     * create a bufferevent and insert into the bufferevent map
     * @param base event base
     * @param fd socket fd
     * @param options
     * @return bufferevent pointer
     */
    struct bufferevent* CreateBufferevent(struct event_base *base, evutil_socket_t fd, int options);

    /**
     * free the bufferevent memory and erase from the bufferevent map
     * @param bev
     */
    void FreeBufferevent(struct bufferevent *bev);

};


#endif //EPIC_CONNECTION_MANAGER_H
