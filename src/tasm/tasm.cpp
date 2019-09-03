// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "tasm.h"

std::string std::to_string(const Tasm::Listing& listing) {
    std::string s = "[";

    for (const uint8_t& op : listing.program) {
        s += op;
        s += " ";
    }

    s += "]";
    s += "( " + HexStr(listing.data) + " )";
    return s;
}
