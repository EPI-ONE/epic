#ifndef __SRC_COIN_H__
#define __SRC_COIN_H__

#include <cstdint>

#include "serialize.h"

static constexpr uint64_t ZERO_COIN       = 0;
static constexpr uint64_t IMPOSSIBLE_COIN = UINT_LEAST64_MAX;

class Coin {
public:
    Coin();

    Coin(uint64_t value);

    Coin(const Coin& coin);

    uint64_t getValue() const;

    void setValue(const uint64_t& value);

    Coin& operator=(uint64_t value);


    Coin& operator+=(uint64_t value);

    Coin& operator-=(uint64_t value);

    Coin& operator*=(uint64_t value);

    Coin& operator<<=(uint64_t value);

    Coin& operator>>=(uint64_t value);

    Coin& operator&=(uint64_t value);

    Coin& operator|=(uint64_t value);


    Coin& operator+=(const Coin& coin);

    Coin& operator-=(const Coin& coin);

    Coin& operator*=(const Coin& coin);

    Coin& operator<<=(const Coin& coin);

    Coin& operator>>=(const Coin& coin);

    Coin& operator&=(const Coin& coin);

    Coin& operator|=(const Coin& coin);


    friend inline bool operator==(const Coin& a, const Coin& b);

    friend inline bool operator!=(const Coin& a, const Coin& b);

    friend inline bool operator<=(const Coin& a, const Coin& b);

    friend inline bool operator>=(const Coin& a, const Coin& b);


    friend inline bool operator==(const Coin& a, uint64_t b);

    friend inline bool operator!=(const Coin& a, uint64_t b);

    friend inline bool operator<=(const Coin& a, uint64_t b);

    friend inline bool operator>=(const Coin& a, uint64_t b);


    friend inline const Coin operator+(const Coin& a, uint64_t b);

    friend inline const Coin operator-(const Coin& a, uint64_t b);

    friend inline const Coin operator*(const Coin& a, uint64_t b);

    friend inline const Coin operator<<(const Coin& a, uint64_t b);

    friend inline const Coin operator>>(const Coin& a, uint64_t b);

    friend inline const Coin operator&(const Coin& a, uint64_t b);

    friend inline const Coin operator|(const Coin& a, uint64_t b);


    friend inline const Coin operator+(const Coin& a, const Coin& b);

    friend inline const Coin operator-(const Coin& a, const Coin& b);

    friend inline const Coin operator*(const Coin& a, const Coin& b);

    friend inline const Coin operator<<(const Coin& a, const Coin& b);

    friend inline const Coin operator>>(const Coin& a, const Coin& b);

    friend inline const Coin operator&(const Coin& a, const Coin& b);

    friend inline const Coin operator|(const Coin& a, const Coin& b);

    ADD_SERIALIZE_METHODS;
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(value_);
    }

private:
    uint64_t value_;
};

inline bool operator==(const Coin& a, const Coin& b) {
    return a.getValue() == b.getValue();
}

inline bool operator!=(const Coin& a, const Coin& b) {
    return a.getValue() != b.getValue();
}

inline bool operator<=(const Coin& a, const Coin& b) {
    return a.getValue() <= b.getValue();
}

inline bool operator>=(const Coin& a, const Coin& b) {
    return a.getValue() >= b.getValue();
}

inline bool operator==(const Coin& a, uint64_t b) {
    return a.getValue() == b;
}

inline bool operator!=(const Coin& a, uint64_t b) {
    return a.getValue() != b;
}

inline bool operator<=(const Coin& a, uint64_t b) {
    return a.getValue() <= b;
}

inline bool operator>=(const Coin& a, uint64_t b) {
    return a.getValue() >= b;
}

inline const Coin operator+(const Coin& a, uint64_t b) {
    return Coin(a) += b;
}

inline const Coin operator-(const Coin& a, uint64_t b) {
    return Coin(a) -= b;
}

inline const Coin operator*(const Coin& a, uint64_t b) {
    return Coin(a) *= b;
}

inline const Coin operator<<(const Coin& a, uint64_t b) {
    return Coin(a) <<= b;
}

inline const Coin operator>>(const Coin& a, uint64_t b) {
    return Coin(a) >>= b;
}

inline const Coin operator&(const Coin& a, uint64_t b) {
    return Coin(a) &= b;
}

inline const Coin operator|(const Coin& a, uint64_t b) {
    return Coin(a) |= b;
}

inline const Coin operator+(const Coin& a, const Coin& b) {
    return Coin(a) += b.getValue();
}

inline const Coin operator-(const Coin& a, const Coin& b) {
    return Coin(a) -= b.getValue();
}

inline const Coin operator*(const Coin& a, const Coin& b) {
    return Coin(a) *= b.getValue();
}

inline const Coin operator<<(const Coin& a, const Coin& b) {
    return Coin(a) <<= b.getValue();
}

inline const Coin operator>>(const Coin& a, const Coin& b) {
    return Coin(a) >>= b.getValue();
}

inline const Coin operator&(const Coin& a, const Coin& b) {
    return Coin(a) &= b.getValue();
}

inline const Coin operator|(const Coin& a, const Coin& b) {
    return Coin(a) |= b.getValue();
}

#endif /* ifndef __SRC_COIN_H__ */
