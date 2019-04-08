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

bool IPAddress::IsRFC1918() const {
    return IsIPv4() && (GetByte(3) == 10 || (GetByte(3) == 192 && GetByte(2) == 168) ||
                           (GetByte(3) == 172 && (GetByte(2) >= 16 && GetByte(2) <= 31)));
}

bool IPAddress::IsRFC2544() const {
    return IsIPv4() && GetByte(3) == 198 && (GetByte(2) == 18 || GetByte(2) == 19);
}

bool IPAddress::IsRFC6598() const {
    return IsIPv4() && GetByte(3) == 100 && GetByte(2) >= 64 && GetByte(2) <= 127;
}

bool IPAddress::IsRFC5737() const {
    return IsIPv4() && ((GetByte(3) == 192 && GetByte(2) == 0 && GetByte(1) == 2) ||
                           (GetByte(3) == 198 && GetByte(2) == 51 && GetByte(1) == 100) ||
                           (GetByte(3) == 203 && GetByte(2) == 0 && GetByte(1) == 113));
}

bool IPAddress::IsRFC3849() const {
    return GetByte(15) == 0x20 && GetByte(14) == 0x01 && GetByte(13) == 0x0D && GetByte(12) == 0xB8;
}

bool IPAddress::IsRFC3927() const {
    return IsIPv4() && (GetByte(3) == 169 && GetByte(2) == 254);
}

bool IPAddress::IsRFC3964() const {
    return (GetByte(15) == 0x20 && GetByte(14) == 0x02);
}

bool IPAddress::IsRFC4380() const {
    return (GetByte(15) == 0x20 && GetByte(14) == 0x01 && GetByte(13) == 0 && GetByte(12) == 0);
}

bool IPAddress::IsRFC4843() const {
    return (GetByte(15) == 0x20 && GetByte(14) == 0x01 && GetByte(13) == 0x00 && (GetByte(12) & 0xF0) == 0x10);
}

bool IPAddress::IsRFC4862() const {
    static const unsigned char pchRFC4862[] = {0xFE, 0x80, 0, 0, 0, 0, 0, 0};
    return (memcmp(ip, pchRFC4862, sizeof(pchRFC4862)) == 0);
}

bool IPAddress::IsRFC6052() const {
    static const unsigned char pchRFC6052[] = {0, 0x64, 0xFF, 0x9B, 0, 0, 0, 0, 0, 0, 0, 0};
    return (memcmp(ip, pchRFC6052, sizeof(pchRFC6052)) == 0);
}

bool IPAddress::IsRFC6145() const {
    static const unsigned char pchRFC6145[] = {0, 0, 0, 0, 0, 0, 0, 0, 0xFF, 0xFF, 0, 0};
    return (memcmp(ip, pchRFC6145, sizeof(pchRFC6145)) == 0);
}

bool IPAddress::IsRoutable() const {
    return IsValid() && !(IsRFC1918() || IsRFC2544() || IsRFC3927() || IsRFC4862() || IsRFC6598() || IsRFC5737() ||
                            IsRFC4843() || IsLocal());
}

bool IPAddress::IsValid() const {
    // unspecified IPv6 address (::/128)
    unsigned char ipNone6[16] = {};
    if (memcmp(ip, ipNone6, 16) == 0)
        return false;

    // documentation IPv6 address
    if (IsRFC3849())
        return false;

    if (IsIPv4()) {
        // INADDR_NONE
        uint32_t ipNone = INADDR_NONE;
        if (memcmp(ip + 12, &ipNone, 4) == 0)
            return false;

        // 0
        ipNone = 0;
        if (memcmp(ip + 12, &ipNone, 4) == 0)
            return false;
    }
    return true;
}

bool IPAddress::IsLocal() const {
    // IPv4 loopback (127.0.0.0/8 or 0.0.0.0/8)
    if (IsIPv4() && (GetByte(3) == 127 || GetByte(3) == 0))
        return true;

    // IPv6 loopback (::1/128)
    static const unsigned char pchLocal[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
    return memcmp(ip, pchLocal, 16) == 0;
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

std::optional<NetAddress> NetAddress::StringToNetAddress(const std::string& ip, const uint16_t& port) {
    std::optional<IPAddress> ipAddress = IPAddress::StringToIP(ip);
    if (ipAddress.has_value()) {
        return NetAddress(*ipAddress, port);
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
