#ifndef __SRC_CRYPTO_SHAHASHVERIFIER__
#define __SRC_CRYPTO_SHAHASHVERIFIER__

#include "crypto/sha_hash_writer.h"

// Reads data from an underlying stream, while hashing the read data.
template<typename Source>
class SHAHashVerifier : public SHAHashWriter {
    private:
        Source* source;
    public:
        explicit SHAHashVerifier(Source* source_) : SHAHashWriter(source_->GetType(), source_->GetVersion()), source(source_) {}

        void read(char* pch, size_t nSize) {
            source->read(pch, nSize);
            this->write(pch, nSize);
        }

        void ignore(size_t nSize) {
            char data[1024];
            while (nSize > 0) {
                size_t now = std::min<size_t>(nSize, 1024);
                read(data, now);
                nSize -= now;
            }
        }
};

#endif
