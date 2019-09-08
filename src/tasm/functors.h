// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef EPIC_FUNCTORS_H
#define EPIC_FUNCTORS_H

#include "pubkey.h"

#include <array>
#include <functional>

typedef std::function<size_t(VStream& data, std::size_t ip)> instruction;

static const std::array<instruction, 256> functors = {
    // FALSE
    ([](VStream&, std::size_t) { return 0; }),
    // TRUE
    ([](VStream&, std::size_t) { return 0; }),
    // VERIFY
    ([](VStream& vdata, std::size_t ip) {
        CPubKey pubkey;
        std::vector<unsigned char> sig;
        uint256 msg;
        std::string encodedAddr;

        try {
            vdata >> pubkey >> sig >> msg >> encodedAddr;
        } catch (std::exception& e) {
            return ip + 1;
        }

        std::optional<CKeyID> addr = DecodeAddress(encodedAddr);
        if (!addr.has_value() || pubkey.GetID() != *addr) {
            return ip + 1;
        }

        if (!pubkey.Verify(msg, sig)) {
            return ip + 1;
        }

        return ip + 2;
    })};

#endif // EPIC_FUNCTORS_H
