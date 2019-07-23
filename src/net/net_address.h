// Copyright (c) 2009-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef EPIC_NET_ADDRESS_H
#define EPIC_NET_ADDRESS_H

#include <arpa/inet.h>
#include <iostream>
#include <netdb.h>
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
public:
    static std::optional<IPAddress> GetByIP(const std::string& ip_string);

    static std::optional<IPAddress> GetByHostname(const std::string& hostname, uint32_t networkFamily);

    static std::optional<IPAddress> GetByHostname(const std::string& hostname);

    IPAddress() = default;

    explicit IPAddress(const struct in_addr& ip4);

    explicit IPAddress(const struct in6_addr& ip6);

    void SetIP(const uint8_t* p, NetworkType type);

    bool IsIPv4() const; // IPv4 mapped address (::FFFF:0:0/96, 0.0.0.0/0)

    std::string ToStringIP() const;

    bool IsRFC1918() const; // IPv4 private networks (10.0.0.0/8, 192.168.0.0/16, 172.16.0.0/12)

    bool IsRFC2544() const; // IPv4 inter-network communications (192.18.0.0/15)

    bool IsRFC6598() const; // IPv4 ISP-level NAT (100.64.0.0/10)

    bool IsRFC5737() const; // IPv4 documentation addresses (192.0.2.0/24, 198.51.100.0/24, 203.0.113.0/24)

    bool IsRFC3849() const; // IPv6 documentation address (2001:0DB8::/32)

    bool IsRFC3927() const; // IPv4 autoconfig (169.254.0.0/16)

    bool IsRFC3964() const; // IPv6 6to4 tunnelling (2002::/16)

    bool IsRFC4380() const; // IPv6 Teredo tunnelling (2001::/32)

    bool IsRFC4843() const; // IPv6 ORCHID (2001:10::/28)

    bool IsRFC4862() const; // IPv6 autoconfig (FE80::/64)

    bool IsRFC6052() const; // IPv6 well-known prefix (64:FF9B::/96)

    bool IsRFC6145() const; // IPv6 IPv4-translated address (::FFFF:0:0:0/96)

    bool IsRoutable() const;

    bool IsLocal() const;

    bool IsValid() const;

    virtual std::string ToString() const;

    uint8_t GetByte(int n) const;

    friend bool operator==(const IPAddress& a, const IPAddress& b);

    friend bool operator!=(const IPAddress& a, const IPAddress& b);

    uint8_t* GetIP() const;

    // this function current only support ipv4
    uint32_t GetIpInt() const;

    virtual ~IPAddress() = default;

protected:
    // Serialize
    unsigned char ip[16];
};

namespace std {
template <>
struct hash<IPAddress> {
    size_t operator()(const IPAddress& f) const {
        return f.GetIpInt();
    }
};
} // namespace std


class NetAddress : public IPAddress {
public:
    static std::optional<NetAddress> GetByIP(const std::string& netaddr_string);

    static std::optional<NetAddress> GetByIP(const std::string& ip, const uint16_t& port);

    static std::optional<NetAddress> GetByHostname(const std::string& netaddr_string);

    static std::optional<NetAddress> GetByHostname(const std::string& hostname, const uint16_t& port);

    NetAddress() = default;

    explicit NetAddress(const IPAddress& ip, uint16_t port);

    explicit NetAddress(const struct in_addr& ip4, uint16_t port);

    explicit NetAddress(const struct in6_addr& ip6, uint16_t port);

    std::string ToString() const override;

    std::string ToStringPort() const;

    uint16_t GetPort() const;

    friend bool operator==(const NetAddress& a, const NetAddress& b);

    friend bool operator!=(const NetAddress& a, const NetAddress& b);

    ADD_SERIALIZE_METHODS
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(ip);
        READWRITE(port_);
    }

protected:
    // Serialize
    uint16_t port_ = 0;
};

namespace std {
template <>
struct hash<NetAddress> {
    size_t operator()(const NetAddress& f) const {
        return f.GetIpInt() * f.GetPort();
    }
};
} // namespace std

#endif // EPIC_NET_ADDRESS_H
