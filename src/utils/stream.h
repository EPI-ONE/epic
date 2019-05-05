// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef __SRC_UTILS_STREAM_H__
#define __SRC_UTILS_STREAM_H__

#include <cstring>

#include "serialize.h"
#include "support/zeroafterfree.h"

// Byte-vector that sets contents to 0's before deallocation.
typedef std::vector<char, zero_after_free_allocator<char>> byte_vector;

/** Double ended buffer combining vector and stream-like interfaces.
 *
 * >> and << read and write unformatted data using the above serialization
 * templates. Fills with data in linear time; some stringstream implementations
 * take N^2 time.
 */
class VStream {
public:
    typedef byte_vector::allocator_type allocator_type;
    typedef byte_vector::size_type size_type;
    typedef byte_vector::difference_type difference_type;
    typedef byte_vector::reference reference;
    typedef byte_vector::const_reference const_reference;
    typedef byte_vector::value_type value_type;
    typedef byte_vector::iterator iterator;
    typedef byte_vector::const_iterator const_iterator;
    typedef byte_vector::reverse_iterator reverse_iterator;

    explicit VStream() : readPos(0) {}

    VStream(size_t size) : chars(size), readPos(0) {}

    VStream(const_iterator pbegin, const_iterator pend) : chars(pbegin, pend), readPos(0) {}

    VStream(const char* pbegin, const char* pend) : chars(pbegin, pend), readPos(0) {}

    VStream(const byte_vector& vchIn) : chars(vchIn.begin(), vchIn.end()), readPos(0) {}

    VStream(const std::vector<char>& vchIn) : chars(vchIn.begin(), vchIn.end()), readPos(0) {}

    VStream(const std::vector<unsigned char>& vchIn) : chars(vchIn.begin(), vchIn.end()), readPos(0) {}

    template <typename... Args>
    VStream(Args&&... args) {
        readPos = 0;
        ::SerializeMany(*this, std::forward<Args>(args)...);
    }

    ADD_SERIALIZE_METHODS;
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(chars);
    }


    VStream& operator+=(const VStream& b) {
        chars.insert(chars.end(), b.cbegin(), b.cend());
        return *this;
    }

    friend VStream operator+(const VStream& a, const VStream& b) {
        VStream ret = a;
        ret += b;
        return (ret);
    }

    friend bool operator==(const VStream& a, const VStream& b) {
        return a.size() == b.size() && (a.empty() || std::memcmp(a.data(), b.data(), a.size()) != 0);
    }

    std::string str() const {
        return std::string(cbegin(), cend());
    }

    // Vector subset
    const_iterator cbegin() const {
        return chars.cbegin() + readPos;
    }

    iterator begin() {
        return chars.begin() + readPos;
    }

    const_iterator cend() const {
        return chars.cend();
    }

    iterator end() {
        return chars.end();
    }

    size_type size() const {
        return chars.size() - readPos;
    }

    bool empty() const {
        return chars.size() == readPos;
    }

    void resize(size_type n, value_type c = 0) {
        chars.resize(n + readPos, c);
    }

    void reserve(size_type n) {
        chars.reserve(n + readPos);
    }

    const_reference operator[](size_type pos) const {
        return chars[pos + readPos];
    }

    reference operator[](size_type pos) {
        return chars[pos + readPos];
    }

    void clear() {
        chars.clear();
        readPos = 0;
    }

    value_type* data() {
        return chars.data() + readPos;
    }

    const value_type* data() const {
        return chars.data() + readPos;
    }

    // Stream subset

    bool eof() const {
        return size() == 0;
    }

    VStream* rdbuf() {
        return this;
    }

    int in_avail() const {
        return size();
    }

    void read(char* pch, size_t nSize) {
        if (nSize == 0)
            return;

        // Read from the beginning of the buffer
        unsigned int nReadPosNext = readPos + nSize;
        if (nReadPosNext > chars.size()) {
            throw std::ios_base::failure("Stream::read(): end of data");
        }
        memcpy(pch, &chars[readPos], nSize);
        if (nReadPosNext == chars.size()) {
            readPos = 0;
            chars.clear();
            return;
        }
        readPos = nReadPosNext;
    }

    void ignore(int nSize) {
        // Ignore from the beginning of the buffer
        if (nSize < 0) {
            throw std::ios_base::failure("Stream::ignore(): nSize negative");
        }
        unsigned int nReadPosNext = readPos + nSize;
        if (nReadPosNext >= chars.size()) {
            if (nReadPosNext > chars.size())
                throw std::ios_base::failure("Stream::ignore(): end of data");
            readPos = 0;
            chars.clear();
            return;
        }
        readPos = nReadPosNext;
    }

    void write(const char* pch, size_t nSize) {
        // Write to the end of the buffer
        chars.insert(chars.end(), pch, pch + nSize);
    }

    template <typename T>
    VStream& operator<<(const T& obj) {
        // Serialize to this stream
        ::Serialize(*this, obj);
        return (*this);
    }

    template <typename T>
    VStream& operator>>(T&& obj) {
        // Deserialize from this stream
        ::Deserialize(*this, obj);
        return (*this);
    }

    void GetAndClear(byte_vector& d) {
        d.insert(d.end(), begin(), end());
        clear();
    }

protected:
    byte_vector chars;
    unsigned int readPos;
};

#endif // ifndef __SRC_UTILS_STREAM_H__
