#ifndef __SRC_UTILS_MAX_QUEUE_H__
#define __SRC_UTILS_MAX_QUEUE_H__

#include <algorithm>
#include <functional>
#include <iterator>
#include <vector>

/**
 * A queue tracking the max, which does NOT allow to erase the current max element
 */
template <typename T, typename Container = std::vector<T>, typename Compare = std::less<typename Container::value_type>>
class max_queue {
public:
    typedef Container container_type;
    typedef Compare value_compare;
    typedef typename container_type::size_type size_type;
    typedef typename container_type::reference reference;
    typedef typename container_type::const_reference const_reference;
    typedef typename container_type::value_type value_type;
    typedef typename container_type::iterator iterator;
    typedef typename container_type::const_iterator const_iterator;
    typedef typename container_type::reverse_iterator reverse_iterator;

private:
    container_type c;
    value_compare comp;
    size_type m;

public:
    max_queue() : c(), comp(), m(0) {}
    max_queue(const max_queue& q) : c(q.c), comp(q.comp), m(q.m) {}
    max_queue& operator=(const max_queue& q) {
        c    = q.c;
        comp = q.comp;
        m    = q.m;
        return *this;
    }
    explicit max_queue(const value_compare& comp_) : c(), comp(comp_), m(0) {}
    max_queue(const value_compare& comp_, const container_type& c_);

    bool empty() const {
        return c.empty();
    }

    size_type size() const {
        return c.size();
    }

    const_reference max() const {
        return c[m];
    }

    void push(const value_type& v) {
        c.push_back(v);
        update_max(c.back(), size() - 1);
    }

    void push(value_type&& v) {
        c.push_back(std::move(v));
        update_max(c.back(), size() - 1);
    }

    template <typename... Args>
    void emplace(Args&&... args) {
        c.emplace_back(std::forward<Args>(args)...);
        update_max(c.back(), size() - 1);
    }

    iterator erase(iterator pos) {
        if (std::distance(c.begin(), pos) == m) {
            // Erasing the current max is not allowed
            return pos;
        }
        return c.erase(pos);
    }

    iterator erase(const_iterator pos) {
        if (std::distance(c.cbegin(), pos) == m) {
            // Erasing the current max is NOT allowed
            return pos;
        }
        return c.erase(pos);
    }

    void pop() {
        // Pop the max element
        c.erase(c.begin() + m);
        // Re-calculate max
        m = std::distance(c.begin(), std::max_element(c.begin(), c.end(), comp));
    }

    const_iterator cbegin() const {
        return c.cbegin();
    }

    const_iterator cend() const {
        return c.cend();
    }

    iterator begin() {
        return c.begin();
    }

    iterator end() {
        return c.end();
    }

    void reserve(size_type n) {
        c.reserve(n);
    }

    void swap(max_queue& q) {
        using std::swap;
        swap(c, q.c);
        swap(comp, q.comp);
        swap(m, q.m);
    }

    void update_max(const_iterator pos) {
        if (comp(max(), *pos)) {
            m = std::distance(c.cbegin(), pos);
        }
    }

private:
    void update_max(const value_type& v, size_type i) {
        if (comp(max(), v)) {
            m = i;
        }
    }
};

class Chain;
typedef max_queue<std::unique_ptr<Chain>,
    std::vector<std::unique_ptr<Chain>>,
    std::function<bool(const std::unique_ptr<Chain>& a, const std::unique_ptr<Chain>& b)>>
    Chains;

#endif /* ifndef __SRC_UTILS_MAX_QUEUE_H__ */
