#ifndef __FUNCTOR_H__
#define __FUNCTOR_H__

#include <array>
#include <functional>
#include <unistd.h>

#include "../utils/stream.h"
#include "pubkey.h"
#include "tasm/tasm.h"
#include "uint256.h"

std::array<instruction, 256> functors = {
    // FALSE
    ([](VStream&, std::size_t) { return 0; }),
    // TRUE
    ([](VStream&, std::size_t) { return 0; }),
    // VERIFY
    ([](VStream& data, std::size_t ip) {
        uint160 pubkeyHash;
        CPubKey pubkey;
        std::vector<unsigned char> sig;
        uint256 msg;
        pubkeyHash.Deserialize(data);
        pubkey.Deserialize(data);
        Deserialize(data, sig);
        msg.Deserialize(data);

        if (Hash160<1>(pubkey.begin(), pubkey.end()) != pubkeyHash) {
            return ip + 1;
        }

        if (!pubkey.Verify(msg, sig)) {
            return ip + 1;
        }

        return ip + 2;
    })};
#endif
