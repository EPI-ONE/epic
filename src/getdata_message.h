#ifndef EPIC_GETDATA_MESSAGE_H
#define EPIC_GETDATA_MESSAGE_H
#include "task.h"

class GetData {
public:
    explicit GetData(GetDataTask::GetDataType type_) : type(type_) {}

    explicit GetData(VStream& stream) {
        Deserialize(stream);
    }

    void AddItem(uint256 hash, uint32_t nonce) {
        blockHashes.emplace_back(hash);
        bundleNonce.emplace_back(nonce);
    }

    // data type
    uint8_t type;

    // block hashes
    std::vector<uint256> blockHashes;

    // random number to track bundle
    std::vector<uint32_t> bundleNonce;

    ADD_SERIALIZE_METHODS;
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(type);
        READWRITE(blockHashes);
        READWRITE(bundleNonce);
    }
};
#endif // EPIC_GETDATA_MESSAGE_H
