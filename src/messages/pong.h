// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef __SRC_PONG_H__
#define __SRC_PONG_H__

#include "serialize.h"

class Pong : public NetMessage {
public:
    uint64_t nonce = 0;

    explicit Pong() : NetMessage(PONG) {}

    explicit Pong(uint64_t nonce) : NetMessage(PONG), nonce(nonce) {}

    explicit Pong(VStream& stream) : NetMessage(PONG) {
        Deserialize(stream);
    }

    ADD_SERIALIZE_METHODS
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(nonce);
    }
    ADD_NET_SERIALIZE_METHODS
};

#endif // EPIC_PONG_H
