#ifndef __FUNCTOR_H__
#define __FUNCTOR_H__

#include <functional>
#include <unistd.h>
#include <array>

#include "../utils/stream.h"

typedef std::function<size_t(VStream& data, std::size_t ip)> instruction;

enum OPCODES {
    RET = 0
};

static std::array<instruction, 256> functors = {
    // 000: exit lambda
    [](VStream& data, std::size_t ip) {
        return ip + 1;
    }
};

#endif
