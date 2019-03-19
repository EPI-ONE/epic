
#include <arpa/inet.h>
#include <string.h>
#include "connection_manager.h"

static new_connection_callback_t new_connection_callback = nullptr;

static void setNewConnectonCallback(new_connection_callback_t func) {
    new_connection_callback = std::move(func);
}

//TODO
static void ReadCallback(struct bufferevent *bev, void *ctx) {

}

//TODO
static void WriteCallback(struct bufferevent *bev, void *ctx) {

}

//TODO
static void EventCallback(struct bufferevent *bev, short events, void *ctx) {

}

std::string Sockaddr2String(struct sockaddr *address) {
    char strAddress[INET_ADDRSTRLEN];
    inet_ntop(address->sa_family, &(((struct sockaddr_in *)address)->sin_addr), strAddress, INET_ADDRSTRLEN);
    return std::string(strAddress)+":"+ std::to_string(ntohs(((struct sockaddr_in *)address)->sin_port));
}



static void AcceptCallback(struct evconnlistener *listener, evutil_socket_t fd, struct sockaddr *address,
                           int socklen, void *ctx) {
    struct event_base *base = evconnlistener_get_base(listener);
    struct bufferevent *bev = bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE);


    if (new_connection_callback != nullptr) {
        new_connection_callback(fd, Sockaddr2String(address));
    }


    bufferevent_setcb(bev, ReadCallback, WriteCallback, EventCallback, NULL);

    bufferevent_enable(bev, EV_READ);

}


ConnectionManager::ConnectionManager() {
    base = event_base_new();
}

ConnectionManager::~ConnectionManager() {
    if (!listener) {
        evconnlistener_free(listener);
    }

    if (!base) {
        event_base_free(base);
    }

}

int ConnectionManager::Listen(uint32_t port, new_connection_callback_t callback_func, uint32_t bindAddress) {

    if(port > 65535) {
        return -1;
    }

    if(!base) {
        return -1;
    }

    setNewConnectonCallback(callback_func);

    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));

    /* This is an INET address */
    sin.sin_family = AF_INET;
    /* Listen on bind address. Default:0.0.0.0 */
    sin.sin_addr.s_addr = htonl(bindAddress);
    /* Listen on the given port. */
    sin.sin_port = htons(port);

    listener = evconnlistener_new_bind(base, AcceptCallback, NULL,
                                       LEV_OPT_CLOSE_ON_FREE | LEV_OPT_REUSEABLE, -1,
                                       (struct sockaddr *) &sin, sizeof(sin));

    if (!listener) {
        return -1;
    }

    event_base_dispatch(base);

    return 0;
}

//TODO
int ConnectionManager::Connect(uint32_t address, uint32_t port) {
    return 0;
}








