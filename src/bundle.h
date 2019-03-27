#ifndef __SRC_BUNDLE_H__
#define __SRC_BUNDLE_H__

#include "block.h"
#include "uint256.h"
#include <vector>

class Bundle {
public:
    std::vector<std::shared_ptr<const Block&>> vblocks;
    std::vector<uint256> hashes;
    uint32_t nonce;
    size_t size;
    uint8_t type;

    Bundle();
    Bundle(std::vector<std::shared_ptr<const Block&>>&& vblocks) : vblocks(std::move(vblocks)) {}
    ~Bundle();

    void AddBlock(std::shared_ptr<const Block&> pblock);
    // Get the first hash of the block vector which should be the milestone
    // block.
    uint256 GetFirstHash();

    // TODO: add serialization methods
};

#endif // __SRC_BUNDLE_H__
