#include "sha_hash_writer.h"

int SHAHashWriter::GetType() const {
    return nType;
}

int SHAHashWriter::GetVersion() const {
    return nVersion;
}

void SHAHashWriter::write(const char *pch, size_t size) {
    ctx.Write((const unsigned char*)pch, size);
}

uint256 SHAHashWriter::GetHash() {
    uint256 result;
    ctx.Finalize((unsigned char*)&result);
    return result;
}

uint64_t SHAHashWriter::GetCheapHash() {
    unsigned char result[SHAHasher256::OUTPUT_SIZE];
    ctx.Finalize(result);
    return ReadLE64(result);
}
