#include "block.h"
#include "hash.h"
#include <ctime>

Block::Block(uint32_t versionNum) {
    version_ = versionNum;
    diffTarget_ = 0x1d07fff8L;
    time_ = std::time(nullptr);
    milestoneBlockHash_ = ZERO_HASH;
    prevBlockHash_ = ZERO_HASH;
    tipBlockHash_ = ZERO_HASH;
}
