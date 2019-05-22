#ifndef EPIC_GETBLOCK_MESSAGE_H
#define EPIC_GETBLOCK_MESSAGE_H

#include <vector>

#include "serialize.h"

class GetBlock {
public:
    explicit GetBlock(std::vector<uint256> locator_, uint32_t nonce_) : locator(std::move(locator_)), nonce(nonce_) {}

    explicit GetBlock(VStream& stream) {
        Deserialize(stream);
    }
    GetBlock(uint32_t nonce_) : nonce(nonce_) {}

    void AddBlockHash(uint256 hash) {
        locator.emplace_back(hash);
    }

    // local milestone hashes
    std::vector<uint256> locator;

    // random number to track sync flow
    uint32_t nonce;

    ADD_SERIALIZE_METHODS;
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(locator);
        READWRITE(nonce);
    }
};
#endif // EPIC_GETBLOCK_MESSAGE_H
