// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "cleanse.h"
#include <cassert>

void memory_cleanse(void* ptr, std::size_t len) {
    assert(ptr != nullptr);
    std::memset(ptr, 0, len);
    /* memory barrier */
    __asm__ __volatile__("" : : "r"(ptr) : "memory");
}
