#ifndef __SRC_UTILS_CHAINS_H__
#define __SRC_UTILS_CHAINS_H__

#include <algorithm>
#include <functional>
#include <iterator>
#include <vector>

#include "chain.h"

class Chains {
private:
    typedef std::vector<ChainPtr> container_type;
    typedef std::function<bool(const ChainPtr&, const ChainPtr&)> value_compare;
    typedef container_type::size_type size_type;
    typedef container_type::reference reference;
    typedef container_type::const_reference const_reference;
    typedef container_type::value_type value_type;
    typedef container_type::iterator iterator;
    typedef container_type::const_iterator const_iterator;

    container_type c;
    size_t m;
    value_compare comp;

public:
    Chains() : c(), m(0), comp([](const ChainPtr& a, const ChainPtr& b) { return *a < *b; }) {}
    Chains(Chains&& q)
        : c(std::move(q.c)), m(q.m), comp([](const ChainPtr& a, const ChainPtr& b) { return *a < *b; }) {}

    bool empty() const {
        return c.empty();
    }

    size_t size() const {
        return c.size();
    }

    const_reference best() const {
        return c[m];
    }

    reference best() {
        return c[m];
    }

    void push(value_type&& v) {
        c.push_back(std::move(v));
        update_best(c.back(), size() - 1);
    }

    template <typename... Args>
    void emplace(Args&&... args) {
        c.emplace_back(std::forward<Args>(args)...);
        update_best(c.back(), size() - 1);
    }

    iterator erase(const_iterator pos) {
        if (std::distance(c.cbegin(), pos) == (long) m) {
            // Erasing the current best is not allowed
            auto it = c.begin();
            std::advance(it, std::distance(c.cbegin(), pos));
            return it;
        }
        if (m > std::distance(c.cbegin(), pos)) {
            m--;
        }
        return c.erase(pos);
    }

    void pop() {
        // Pop the best element
        c.erase(c.begin() + m);
        // Re-calculate the best
        m = std::distance(c.begin(), std::max_element(c.begin(), c.end(), comp));
    }

    iterator begin() {
        return c.begin();
    }

    iterator end() {
        return c.end();
    }

    const_iterator begin() const {
        return c.cbegin();
    }

    const_iterator end() const {
        return c.cend();
    }

    void reserve(size_type n) {
        c.reserve(n);
    }

    void update_best(const_iterator pos) {
        if (comp(best(), *pos)) {
            c[m]->ismainchain_ = false;
            m = std::distance(c.cbegin(), pos);
            c[m]->ismainchain_ = true;
        }
    }

private:
    void update_best(const value_type& v, size_type i) {
        if (comp(best(), v)) {
            c[m]->ismainchain_ = false;
            m = i;
            c[m]->ismainchain_ = true;
        }
    }
};

#endif /* ifndef __SRC_UTILS_CHAINS_H__ */
