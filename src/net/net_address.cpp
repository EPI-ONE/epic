// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#include "net_address.h"

// for IPv4 address (4 bytes) expressed in 16 bytes, the first 12 bytes should be set to the value below
static const unsigned char pchIPv4[12] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xff};

IPAddress::IPAddress(const struct in_addr& ip4) {
    setIP((const uint8_t*) &(ip4.s_addr), NET_IPV4);
}

IPAddress::IPAddress(const struct in6_addr& ip6) {
    setIP((const uint8_t*) &(ip6.s6_addr), NET_IPV6);
    //    setIP((const uint8_t *) &(ip6.__u6_addr.__u6_addr8), NET_IPV6);
}

std::optional<IPAddress> IPAddress::StringToIP(const std::string& ip_string) {
    if (ip_string.find(':') == std::string::npos) {
        struct sockaddr_in sa;
        if (inet_pton(AF_INET, ip_string.c_str(), &sa.sin_addr) != 0) {
            return IPAddress(sa.sin_addr);
        }
    } else {
        struct sockaddr_in6 sa;
        if (inet_pton(AF_INET6, ip_string.c_str(), &sa.sin6_addr) != 0) {
            return IPAddress(sa.sin6_addr);
        }
    }
    return {};
}

void IPAddress::setIP(const uint8_t* p, NetworkType type) {
    switch (type) {
    case NET_IPV4:
        memcpy(ip, pchIPv4, 12);
        memcpy(ip + 12, p, 4);
        break;
    case NET_IPV6:
        memcpy(ip, p, 16);
        break;
    default:
        assert("invalid network");
    }
}

bool IPAddress::IsIPv4() const {
    return memcmp(ip, pchIPv4, sizeof(pchIPv4)) == 0;
}

unsigned char IPAddress::GetByte(int index) const {
    assert(index >= 0 && index <= 15);
    return ip[15 - index];
}

bool operator==(const IPAddress& a, const IPAddress& b) {
    return memcmp(a.ip, b.ip, 16) == 0;
}

bool operator!=(const IPAddress& a, const IPAddress& b) {
    return !(a == b);
}

unsigned char* IPAddress::GetIP() const {
    return (unsigned char*) ip;
}

std::string IPAddress::ToStringIP() const {
    if (IsIPv4()) {
        return strprintf("%u.%u.%u.%u", GetByte(3), GetByte(2), GetByte(1), GetByte(0));
    } else {
        return strprintf("%x:%x:%x:%x:%x:%x:%x:%x", GetByte(15) << 8 | GetByte(14), GetByte(13) << 8 | GetByte(12),
            GetByte(11) << 8 | GetByte(10), GetByte(9) << 8 | GetByte(8), GetByte(7) << 8 | GetByte(6),
            GetByte(5) << 8 | GetByte(4), GetByte(3) << 8 | GetByte(2), GetByte(1) << 8 | GetByte(0));
    }
}

std::string IPAddress::ToString() const {
    return ToStringIP();
}

uint32_t IPAddress::GetIpInt() const {
    return GetByte(3) << 24 | GetByte(2) << 16 | GetByte(1) << 8 | GetByte(0);
}


NetAddress::NetAddress(const IPAddress& ip, uint16_t port) : IPAddress(ip), port_(port) {
}

NetAddress::NetAddress(const struct in_addr& ip4, uint16_t port) : IPAddress(ip4), port_(port) {
}

NetAddress::NetAddress(const struct in6_addr& ip6, uint16_t port) : IPAddress(ip6), port_(port) {
}

std::optional<NetAddress> NetAddress::StringToNetAddress(const std::string& netaddr_string) {
    std::string ip;
    int port;
    SplitHostPort(netaddr_string, port, ip);
    if (port != -1) {
        std::optional<IPAddress> ipAddress = IPAddress::StringToIP(ip);
        if (ipAddress.has_value()) {
            return NetAddress(*ipAddress, port);
        }
    }
    return {};
}

std::string NetAddress::ToString() const {
    if (IsIPv4()) {
        return ToStringIP() + ":" + ToStringPort();
    }
    return "[" + ToStringIP() + "]" + ":" + ToStringPort();
}

uint16_t NetAddress::GetPort() const {
    return port_;
}

std::string NetAddress::ToStringPort() const {
    return strprintf("%u", port_);
}

bool operator==(const NetAddress& a, const NetAddress& b) {
    return memcmp(a.ip, b.ip, 16) == 0 && a.port_ == b.port_;
}

bool operator!=(const NetAddress& a, const NetAddress& b) {
    return !(a == b);
}
