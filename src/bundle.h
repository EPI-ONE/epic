#ifndef __SRC_BUNDLE_H__
#define __SRC_BUNDLE_H__

#include <block.h>

class Block;

class Bundle {
    public:
        std::vector<Block> blocks;
        std::vector<uint256> hashes;
        long nonce;
        int size;
        char type;

    public:
        Bundle();
        Bundle(const std::vector<Block>& blocks);

        void AddBlock(Block& block);
        uint256 GetFirstHash();

        // TODO: add serialization methods

        ~Bundle();
};

#endif // __SRC_BUNDLE_H__
