#ifndef EPIC_NET_ADDRESS_H
#define EPIC_NET_ADDRESS_H

#include <vector>
#include <string>
#include <sstream>


enum NetworkType {
    NET_UNROUTABLE = 0,
    NET_IPV4,
//    NET_IPV6,
//    NET_ONION,
            NET_INTERNAL,

//    NET_MAX,
};

class IPAddress {
    protected:
        unsigned char ip[16];

    public:
        IPAddress() = default;

        explicit IPAddress(const std::string &ip_string);

        bool setIP(const std::string& ip_string);

        bool IsIPv4() const;    // IPv4 mapped address (::FFFF:0:0/96, 0.0.0.0/0)

        std::string ToStringIP() const;

        std::string ToString() const;

        unsigned char GetByte(int n) const;

        friend bool operator==(const IPAddress &a, const IPAddress &b);

        friend bool operator!=(const IPAddress &a, const IPAddress &b);

//        friend bool operator<(const IPAddress &a, const IPAddress &b);

        enum NetworkType GetNetworkType() const;

        unsigned char *GetIP() const;
};

class NetAddress : public IPAddress {
    protected:
        uint16_t port;

    public:
        explicit NetAddress(std::string address_string);

        std::string ToString() const;

        std::string ToStringPort() const;

        uint16_t GetPort() const;

};


#endif //EPIC_NET_ADDRESS_H
