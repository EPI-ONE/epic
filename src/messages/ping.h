// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef EPIC_PING_H
#define EPIC_PING_H

#include "net_message.h"
#include "serialize.h"

class Ping : public NetMessage {
public:
    // Serialize
    uint64_t nonce = 0;

    explicit Ping() : NetMessage(PING) {}

    explicit Ping(uint64_t nonce) : NetMessage(PING), nonce(nonce) {}

    explicit Ping(VStream& stream) : NetMessage(PING) {
        Deserialize(stream);
    }

    ADD_SERIALIZE_METHODS
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(nonce);
    }
    ADD_NET_SERIALIZE_METHODS
};

#endif // EPIC_PING_H
