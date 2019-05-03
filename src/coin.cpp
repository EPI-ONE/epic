#include "coin.h"

Coin::Coin() {
    value_ = ZERO_COIN;
}

Coin::Coin(uint64_t value) {
    setValue(value);
}

Coin::Coin(const Coin& coin) {
    setValue(coin.getValue());
}

uint64_t Coin::getValue() const {
    return value_;
}

void Coin::setValue(const uint64_t& value) {
    value_ = value;
}

Coin& Coin::operator=(uint64_t value) {
    setValue(value);
    return *this;
}

Coin& Coin::operator+=(uint64_t value) {
    value_ += value;
    return *this;
}

Coin& Coin::operator-=(uint64_t value) {
    value_ -= value;
    return *this;
}

Coin& Coin::operator*=(uint64_t value) {
    value_ *= value;
    return *this;
}

Coin& Coin::operator<<=(uint64_t value) {
    value_ <<= value;
    return *this;
}

Coin& Coin::operator>>=(uint64_t value) {
    value_ >>= value;
    return *this;
}

Coin& Coin::operator&=(uint64_t value) {
    value_ &= value;
    return *this;
}

Coin& Coin::operator|=(uint64_t value) {
    value_ |= value;
    return *this;
}


Coin& Coin::operator+=(const Coin& coin) {
    value_ += coin.getValue();
    return *this;
}

Coin& Coin::operator-=(const Coin& coin) {
    value_ -= coin.getValue();
    return *this;
}

Coin& Coin::operator*=(const Coin& coin) {
    value_ *= coin.getValue();
    return *this;
}

Coin& Coin::operator<<=(const Coin& coin) {
    value_ <<= coin.getValue();
    return *this;
}

Coin& Coin::operator>>=(const Coin& coin) {
    value_ <<= coin.getValue();
    return *this;
}

Coin& Coin::operator&=(const Coin& coin) {
    value_ &= coin.getValue();
    return *this;
}

Coin& Coin::operator|=(const Coin& coin) {
    value_ |= coin.getValue();
    return *this;
}
