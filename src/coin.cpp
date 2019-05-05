#include "coin.h"

Coin::Coin() {
    value_ = ZERO_COIN;
}

Coin::Coin(uint64_t value) {
    SetValue(value);
}

Coin::Coin(const Coin& coin) {
    SetValue(coin.GetValue());
}

uint64_t Coin::GetValue() const {
    return value_;
}

void Coin::SetValue(const uint64_t& value) {
    value_ = value;
}

Coin& Coin::operator=(uint64_t value) {
    SetValue(value);
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
    value_ += coin.GetValue();
    return *this;
}

Coin& Coin::operator-=(const Coin& coin) {
    value_ -= coin.GetValue();
    return *this;
}

Coin& Coin::operator*=(const Coin& coin) {
    value_ *= coin.GetValue();
    return *this;
}

Coin& Coin::operator<<=(const Coin& coin) {
    value_ <<= coin.GetValue();
    return *this;
}

Coin& Coin::operator>>=(const Coin& coin) {
    value_ <<= coin.GetValue();
    return *this;
}

Coin& Coin::operator&=(const Coin& coin) {
    value_ &= coin.GetValue();
    return *this;
}

Coin& Coin::operator|=(const Coin& coin) {
    value_ |= coin.GetValue();
    return *this;
}
