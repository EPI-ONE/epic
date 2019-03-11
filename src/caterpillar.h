#ifndef __SRC_CATERPILLAR_H__
#define __SRC_CATERPILLAR_H__

#include <vector>
#include <utils/uint256.h>

class Block;

class Caterpillar {
    public:
        // will return a pointer-like structure later
        Block* GetBlock(const uint256 bhash) const;
        // flush blocks to db + file storage system
        void Store(const std::vector<Block*> vblock);
};

#endif // __SRC_CATERPILLAR_H__
