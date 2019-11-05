// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef EPIC_CIRCULAR_QUEUE_H
#define EPIC_CIRCULAR_QUEUE_H

#include "serialize.h"

#include <deque>

template <typename T>
class CircularQueue : public std::deque<T> {
    typedef std::deque<T> base;

public:
    CircularQueue() : base(), cap(UINT64_MAX) {}
    explicit CircularQueue(size_t s) : base(), cap(s) {}

    void pop() {
        if (!base::empty()) {
            base::pop_back();
        }
    }

    void push(const T& value) {
        if (base::size() == cap) {
            pop();
        }
        base::push_front(value);
    }

    void push(T&& value) {
        if (base::size() == cap) {
            pop();
        }
        base::push_front(value);
    }

    void set_limit(size_t s) {
        cap = s;
        while (base::size() > s) {
            pop();
        }
    }

    ADD_SERIALIZE_METHODS
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITEAS(base, *this);
    }

private:
    size_t cap;
};

#endif // EPIC_CIRCULAR_QUEUE_H
