// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_HASH_H
#define BITCOIN_HASH_H

// TODO: add supports to ripemd160, serialization and parameter
#include "crypto/common.h"
#include "crypto/sha256.h"
#include "uint256.h"

#include <vector>

typedef uint256 ChainCode;

/** A hasher class for Bitcoin's 256-bit hash (double SHA-256). */
class CHash256 {
private:
    CSHA256 sha;
public:
    static const size_t OUTPUT_SIZE = CSHA256::OUTPUT_SIZE;

    void Finalize(unsigned char hash[OUTPUT_SIZE]) {
        unsigned char buf[CSHA256::OUTPUT_SIZE];
        sha.Finalize(buf);
        sha.Reset().Write(buf, CSHA256::OUTPUT_SIZE).Finalize(hash);
    }

    CHash256& Write(const unsigned char *data, size_t len) {
        sha.Write(data, len);
        return *this;
    }

    CHash256& Reset() {
        sha.Reset();
        return *this;
    }
};

/** Compute the 256-bit hash of an object. */
template<typename T1>
inline uint256 Hash(const T1 pbegin, const T1 pend)
{
    static const unsigned char pblank[1] = {};
    uint256 result;
    CHash256().Write(pbegin == pend ? pblank : (const unsigned char*)&pbegin[0], (pend - pbegin) * sizeof(pbegin[0]))
              .Finalize((unsigned char*)&result);
    return result;
}

/** Compute the 256-bit hash of the concatenation of two objects. */
template<typename T1, typename T2>
inline uint256 Hash(const T1 p1begin, const T1 p1end,
                    const T2 p2begin, const T2 p2end) {
    static const unsigned char pblank[1] = {};
    uint256 result;
    CHash256().Write(p1begin == p1end ? pblank : (const unsigned char*)&p1begin[0], (p1end - p1begin) * sizeof(p1begin[0]))
              .Write(p2begin == p2end ? pblank : (const unsigned char*)&p2begin[0], (p2end - p2begin) * sizeof(p2begin[0]))
              .Finalize((unsigned char*)&result);
    return result;
}

/** A writer stream (for serialization) that computes a 256-bit hash. */
class CHashWriter
{
private:
    CHash256 ctx;

    const int nType;
    const int nVersion;
public:

    CHashWriter(int nTypeIn, int nVersionIn) : nType(nTypeIn), nVersion(nVersionIn) {}

    int GetType() const { return nType; }
    int GetVersion() const { return nVersion; }

    void write(const char *pch, size_t size) {
        ctx.Write((const unsigned char*)pch, size);
    }

    // invalidates the object
    uint256 GetHash() {
        uint256 result;
        ctx.Finalize((unsigned char*)&result);
        return result;
    }

    /**
     * Returns the first 64 bits from the resulting hash.
     */
    inline uint64_t GetCheapHash() {
        unsigned char result[CHash256::OUTPUT_SIZE];
        ctx.Finalize(result);
        return ReadLE64(result);
    }

};

/** Reads data from an underlying stream, while hashing the read data. */
template<typename Source>
class CHashVerifier : public CHashWriter
{
private:
    Source* source;

public:
    explicit CHashVerifier(Source* source_) : CHashWriter(source_->GetType(), source_->GetVersion()), source(source_) {}

    void read(char* pch, size_t nSize)
    {
        source->read(pch, nSize);
        this->write(pch, nSize);
    }

    void ignore(size_t nSize)
    {
        char data[1024];
        while (nSize > 0) {
            size_t now = std::min<size_t>(nSize, 1024);
            read(data, now);
            nSize -= now;
        }
    }

};

#endif // BITCOIN_HASH_H
