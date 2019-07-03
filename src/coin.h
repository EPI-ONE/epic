#ifndef __SRC_COIN_H__
#define __SRC_COIN_H__

#include <cstdint>

#include "serialize.h"

static constexpr uint64_t ZERO_COIN       = 0;
static constexpr uint64_t IMPOSSIBLE_COIN = UINT_LEAST64_MAX;

class Coin {
public:
    Coin() : value_(ZERO_COIN) {}

    Coin(uint64_t value) : value_(value) {}

    Coin(const Coin& coin) : value_(coin.value_) {}

    uint64_t GetValue() const {
        return value_;
    }

    ADD_SERIALIZE_METHODS;
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(VARINT(value_));
    }

    Coin& operator=(uint64_t value) {
        value_ = value;
        return *this;
    }

    Coin& operator+=(uint64_t value) {
        value_ += value;
        return *this;
    }

    Coin& operator-=(uint64_t value) {
        value_ -= value;
        return *this;
    }

    Coin& operator+=(const Coin& coin) {
        value_ += coin.value_;
        return *this;
    }

    Coin& operator-=(const Coin& coin) {
        value_ -= coin.value_;
        return *this;
    }

    bool operator==(uint64_t value) const {
        return value_ == value;
    }

    bool operator==(const Coin& another) const {
        return value_ == another.value_;
    }

    bool operator!=(uint64_t value) const {
        return value_ != value;
    }

    bool operator!=(const Coin& another) const {
        return value_ != another.value_;
    }

    bool operator>=(uint64_t value) const {
        return value_ >= value;
    }

    bool operator>=(const Coin& another) const {
        return value_ >= another.value_;
    }

    bool operator<=(uint64_t value) const {
        return value_ <= value;
    }

    bool operator<=(const Coin& another) const {
        return value_ <= another.value_;
    }

    bool operator<(uint64_t value) const {
        return value_ < value;
    }

    bool operator<(const Coin& another) const {
        return value_ < another.value_;
    }

    bool operator>(uint64_t value) const {
        return value_ > value;
    }

    bool operator>(const Coin& another) const {
        return value_ > another.value_;
    }

    friend inline Coin operator+(const Coin& a, uint64_t b) {
        return Coin(a.value_ + b);
    }

    friend inline Coin operator-(const Coin& a, uint64_t b) {
        return Coin(a.value_ - b);
    }

    friend inline Coin operator+(const Coin& a, const Coin& b) {
        return Coin(a.value_ + b.value_);
    }

    friend inline Coin operator-(const Coin& a, const Coin& b) {
        return Coin(a.value_ - b.value_);
    }

    friend inline Coin operator*(const Coin& a, uint32_t multiple) {
        if (multiple == 0) {
            return Coin{};
        }

        // TODO: consider how to handle the exception
        if (a.value_ > IMPOSSIBLE_COIN / multiple) {
            throw std::string("Coin number overflow");
        }
        return Coin(a.value_ * multiple);
    }

private:
    uint64_t value_;
};

#endif /* ifndef __SRC_COIN_H__ */
