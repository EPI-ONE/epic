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

    explicit VStream() : readPos_(0) {}

    VStream(size_t size) : chars_(size), readPos_(0) {}

    VStream(const_iterator pbegin, const_iterator pend) : chars_(pbegin, pend), readPos_(0) {}

    VStream(const char* pbegin, const char* pend) : chars_(pbegin, pend), readPos_(0) {}

    VStream(const byte_vector& vchIn) : chars_(vchIn.begin(), vchIn.end()), readPos_(0) {}

    VStream(const std::vector<char>& vchIn) : chars_(vchIn.begin(), vchIn.end()), readPos_(0) {}

    VStream(const std::vector<unsigned char>& vchIn) : chars_(vchIn.begin(), vchIn.end()), readPos_(0) {}

    template <typename... Args>
    VStream(Args&&... args) {
        readPos_ = 0;
        ::SerializeMany(*this, std::forward<Args>(args)...);
    }

    ADD_SERIALIZE_METHODS;
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(chars_);
    }


    VStream& operator+=(const VStream& b) {
        chars_.insert(chars_.end(), b.cbegin(), b.cend());
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
        return chars_.cbegin() + readPos_;
    }

    iterator begin() {
        return chars_.begin() + readPos_;
    }

    const_iterator cend() const {
        return chars_.cend();
    }

    iterator end() {
        return chars_.end();
    }

    size_type size() const {
        return chars_.size() - readPos_;
    }

    bool empty() const {
        return chars_.size() == readPos_;
    }

    void resize(size_type n, value_type c = 0) {
        chars_.resize(n + readPos_, c);
    }

    void reserve(size_type n) {
        chars_.reserve(n + readPos_);
    }

    const_reference operator[](size_type pos) const {
        return chars_[pos + readPos_];
    }

    reference operator[](size_type pos) {
        return chars_[pos + readPos_];
    }

    void clear() {
        chars_.clear();
        readPos_ = 0;
    }

    value_type* data() {
        return chars_.data() + readPos_;
    }

    const value_type* data() const {
        return chars_.data() + readPos_;
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
        unsigned int nReadPosNext = readPos_ + nSize;
        if (nReadPosNext > chars_.size()) {
            throw std::ios_base::failure("VStream::read(): end of data");
        }

        memcpy(pch, &chars_[readPos_], nSize);
        if (nReadPosNext == chars_.size()) {
            readPos_ = 0;
            chars_.clear();
            return;
        }

        readPos_ = nReadPosNext;
    }

    void ignore(int nSize) {
        // Ignore from the beginning of the buffer
        if (nSize < 0) {
            throw std::ios_base::failure("VStream::ignore(): nSize negative");
        }

        unsigned int nReadPosNext = readPos_ + nSize;
        if (nReadPosNext >= chars_.size()) {
            if (nReadPosNext > chars_.size())
                throw std::ios_base::failure("VStream::ignore(): end of data");
            readPos_ = 0;
            chars_.clear();
            return;
        }

        readPos_ = nReadPosNext;
    }

    void write(const char* pch, size_t nSize) {
        // Write to the end of the buffer
        // size_t end = size();
        // chars_.resize(end + nSize);
        // memcpy(&chars_[end], pch, nSize);
        chars_.insert(chars_.end(), pch, pch + nSize);
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
    byte_vector chars_;
    unsigned int readPos_;
};

#endif // ifndef __SRC_UTILS_STREAM_H__
