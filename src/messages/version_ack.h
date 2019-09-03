// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef EPIC_VERSION_ACK_H
#define EPIC_VERSION_ACK_H

#include "serialize.h"

class VersionACK {
public:
    ADD_SERIALIZE_METHODS
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream&, Operation) {}
};

#endif // EPIC_VERSION_ACK_H
