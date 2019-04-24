#ifndef __FUNCTOR_H__
#define __FUNCTOR_H__

#include <functional>
#include <unistd.h>
#include <array>

typedef std::function<size_t(std::vector<unsigned char>& data, std::size_t ip)> instruction;

enum OPCODES {
    RET = 0
};

static std::array<instruction, 256> functors = {
    // 000: exit lambda
    [](std::vector<unsigned char>& data, std::size_t ip) {
                                        return ip + 1;
    }
};

#endif
