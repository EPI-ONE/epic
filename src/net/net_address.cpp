#include <iostream>
#include "net_address.h"

static const unsigned char pchIPv4[12] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xff};

IPAddress::IPAddress(const std::string &ip_string) {
    setIP(ip_string);
}

bool IPAddress::IsIPv4() const {
    return (memcmp(ip, pchIPv4, sizeof(pchIPv4)) == 0);
}

unsigned char IPAddress::GetByte(int index) const {
    return ip[15 - index];
}

bool operator==(const IPAddress &a, const IPAddress &b) {
    return memcmp(a.ip, b.ip, 16) == 0;
}

bool operator!=(const IPAddress &a, const IPAddress &b) {
    return !(a == b);
}

unsigned char *IPAddress::GetIP() const {
    return (unsigned char *) ip;
}


std::string IPAddress::ToStringIP() const {
    if (IsIPv4()) {
        return strprintf("%u.%u.%u.%u", GetByte(3), GetByte(2), GetByte(1), GetByte(0));
    }
}

std::string IPAddress::ToString() const {
    return ToStringIP();
}

bool IPAddress::setIP(const std::string &ip_string) {
    int dot_size = std::count(ip_string.begin(), ip_string.end(), '.');
    if (dot_size == 3) {
        std::stringstream ss(ip_string);
        char dot;
        int p;
        for (int i = 0; i < 4; i++) {
            ss >> p;
            if (p > 255 || p < 0) {
                std::cout << "invalid ip string: " << ip_string << std::endl;
                return false;
            }
            ip[12 + i] = (unsigned char) p;
            if (i != 3) {
                ss >> dot;
            }
        }
        memcpy(ip, pchIPv4, 12);
        return true;
    }
    return false;
}


std::string NetAddress::ToString() const {
    if (IsIPv4()) {
        return "[" + ToStringIP() + ":" + ToStringPort()+"]";
    }
}

uint16_t NetAddress::GetPort() const {
    return port;
}

std::string NetAddress::ToStringPort() const {
    return strprintf("%u", port);
}

NetAddress::NetAddress(std::string address_string) {
    int index = address_string.find(':');
    setIP(address_string.substr(0,index));
    port = (uint16_t)std::stoi(address_string.substr(index+1));
}
