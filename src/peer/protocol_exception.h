// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef EPIC_PROTOCOL_EXCEPTION_H
#define EPIC_PROTOCOL_EXCEPTION_H

#include <exception>
#include <string>
#include <utility>

class ProtocolException : public std::exception {
public:
    explicit ProtocolException(std::string msg) : msg_(std::move(msg)) {}

    std::string ToString() const {
        return msg_;
    }

private:
    std::string msg_;
};

#endif // EPIC_PROTOCOL_EXCEPTION_H
