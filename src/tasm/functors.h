#ifndef __FUNCTOR_H__
#define __FUNCTOR_H__

#include <array>
#include <functional>
#include <unistd.h>

#include "pubkey.h"

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
#endif
