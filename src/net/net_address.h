// Copyright (c) 2009-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef EPIC_NET_ADDRESS_H
#define EPIC_NET_ADDRESS_H

#include <arpa/inet.h>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <tinyformat.h>

#include "serialize.h"
#include "utilstrencodings.h"

enum NetworkType {
    NET_UNROUTABLE = 0,
    NET_IPV4,
    NET_IPV6,
    NET_LOCAL,
};

class IPAddress {
protected:
    // Serialize
    unsigned char ip[16];

public:
    static std::optional<IPAddress> StringToIP(const std::string& ip_string);

    IPAddress() = default;

    explicit IPAddress(const struct in_addr& ip4);

    explicit IPAddress(const struct in6_addr& ip6);

    void setIP(const uint8_t* p, NetworkType type);

    bool IsIPv4() const; // IPv4 mapped address (::FFFF:0:0/96, 0.0.0.0/0)

    std::string ToStringIP() const;

    virtual std::string ToString() const;

    uint8_t GetByte(int n) const;

    friend bool operator==(const IPAddress& a, const IPAddress& b);

    friend bool operator!=(const IPAddress& a, const IPAddress& b);

    uint8_t* GetIP() const;

    // this function current only support ipv4
    uint32_t GetIpInt() const;
};

class NetAddress : public IPAddress {
protected:
    // Serialize
    uint16_t port_ = 0;

public:
    static std::optional<NetAddress> StringToNetAddress(const std::string& netaddr_string);

    NetAddress() = default;

    explicit NetAddress(const IPAddress& ip, uint16_t port);

    explicit NetAddress(const struct in_addr& ip4, uint16_t port);

    explicit NetAddress(const struct in6_addr& ip6, uint16_t port);

    std::string ToString() const override;

    std::string ToStringPort() const;

    uint16_t GetPort() const;

    friend bool operator==(const NetAddress& a, const NetAddress& b);

    friend bool operator!=(const NetAddress& a, const NetAddress& b);

    ADD_SERIALIZE_METHODS;
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(ip);
        READWRITE(port_);
    }
};

#endif // EPIC_NET_ADDRESS_H
