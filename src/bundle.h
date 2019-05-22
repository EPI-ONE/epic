#ifndef __SRC_BUNDLE_H__
#define __SRC_BUNDLE_H__

#include <vector>

#include "block.h"
#include "serialize.h"
#include "uint256.h"

class Bundle {
public:
    explicit Bundle(VStream& stream) {
        Deserialize(stream);
    }

    explicit Bundle(uint32_t nonce_) : nonce(nonce_) {}

    explicit Bundle(std::vector<ConstBlockPtr> blocks_, uint32_t nonce_) : blocks(std::move(blocks_)), nonce(nonce_) {}

    Bundle(Bundle&& other) : blocks(std::move(other.blocks)), nonce(other.nonce) {}

    void AddBlock(const ConstBlockPtr& blockPtr) {
        blocks.push_back(blockPtr);
    }

    // max block size of a bundle
    const static size_t kMaxBlockSize = 100000;

    // block
    std::vector<ConstBlockPtr> blocks;

    // nonce
    uint32_t nonce;

    // TODO: add serialization methods
    ADD_SERIALIZE_METHODS;
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(blocks);
    }
};

#endif // __SRC_BUNDLE_H__
