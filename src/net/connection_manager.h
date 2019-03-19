
#ifndef EPIC_CONNECTION_MANAGER_H
#define EPIC_CONNECTION_MANAGER_H


#include <cstdint>
#include <event2/listener.h>
#include <event2/event.h>
#include <event2/bufferevent.h>
#include <event2/thread.h>
#include <functional>
#include <string>
#include <thread>

typedef std::function<void(evutil_socket_t socket_id, std::string address)> new_connection_callback_t;
typedef std::function<void(evutil_socket_t socket_id)> delete_connection_callback_t;

class ConnectionManager {

private:
    struct event_base *base = nullptr;
    struct evconnlistener *listener = nullptr;

    std::thread thread_event;

public:
    ConnectionManager();
    virtual ~ConnectionManager();

    int Listen(uint32_t port, uint32_t bindAddress = 0);
    int Connect(uint32_t address, uint32_t port);
    void RegisterNewConnectionCallback(new_connection_callback_t callback_func);
    void Start();
    void Stop();


};


#endif //EPIC_CONNECTION_MANAGER_H
