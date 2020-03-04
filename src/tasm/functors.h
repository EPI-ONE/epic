// Copyright (c) 2020 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef EPIC_FUNCTORS_H
#define EPIC_FUNCTORS_H

#include "pubkey.h"

#include <array>
#include <functional>
#include <unordered_set>

namespace tasm {

using instruction = std::function<size_t(VStream& data, std::size_t ip)>;

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
    }),
    // MULTISIG: select m from n
    ([](VStream& vdata, std::size_t ip) {
        uint8_t m;
        std::vector<std::pair<CPubKey, std::pair<std::vector<unsigned char>, uint256>>> vin{};
        std::vector<std::string> vEncAddr{};

        try {
            vdata >> vin >> m >> vEncAddr;
        } catch (std::exception& e) {
            return ip + 1;
        }

        if (vin.size() != m) {
            return ip + 1;
        }

        // decode addresses to a set
        std::unordered_set<CKeyID> sAddr{};
        sAddr.reserve(vEncAddr.size());

        for (const auto& enc : vEncAddr) {
            if (auto addr = DecodeAddress(enc)) {
                sAddr.emplace(std::move(*addr));
            } else {
                return ip + 1;
            }
        }

        // verification
        for (const auto& [pubkey, info] : vin) {
            if (sAddr.find(pubkey.GetID()) == sAddr.end()) {
                return ip + 1;
            }

            if (!pubkey.Verify(info.second, info.first)) {
                return ip + 1;
            }
        }

        return ip + 2;
    })};

} // namespace tasm

#endif // EPIC_FUNCTORS_H
