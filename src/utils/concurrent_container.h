#ifndef __SRC_UTILS_CONCURRENT_HASHMAP_H__
#define __SRC_UTILS_CONCURRENT_HASHMAP_H__

#include <functional>
#include <queue>
#include <shared_mutex>
#include <unordered_map>
#include <unordered_set>

#define READER_LOCK(mu) std::shared_lock<std::shared_mutex> reader(mu);
#define WRITER_LOCK(mu) std::unique_lock<std::shared_mutex> writer(mu);

template <typename Container>
class ConcurrentContainer {
public:
    typedef Container container_type;
    typedef typename container_type::value_type value_type;
    typedef typename container_type::size_type size_type;
    typedef typename container_type::iterator iterator;
    typedef typename container_type::const_iterator const_iterator;
    typedef value_type& reference;
    typedef const value_type& const_reference;

    ConcurrentContainer() : mutex_(), c() {}
    ConcurrentContainer(const ConcurrentContainer& m) : mutex_(), c(m.c) {}
    ConcurrentContainer(ConcurrentContainer&& m) noexcept : mutex_(), c(std::move(m.c)) {}
    ConcurrentContainer(std::initializer_list<value_type> l) : c(l) {}
    ~ConcurrentContainer() = default;

    ConcurrentContainer& operator=(const ConcurrentContainer& m) {
        WRITER_LOCK(mutex_)
        c = m.c;
        return *this;
    }

    ConcurrentContainer& operator=(ConcurrentContainer&& m) noexcept {
        WRITER_LOCK(mutex_)
        c = std::move(m.c);
        return *this;
    }

    bool empty() const {
        READER_LOCK(mutex_)
        return c.empty();
    }

    size_type size() const {
        READER_LOCK(mutex_)
        return c.size();
    }

    const_iterator begin() const {
        READER_LOCK(mutex_)
        return c.cbegin();
    }

    const_iterator end() const {
        READER_LOCK(mutex_)
        return c.cend();
    }

    template <class... Args>
    std::pair<iterator, bool> emplace(Args&&... args) {
        WRITER_LOCK(mutex_)
        return c.emplace(std::forward<Args>(args)...);
    }

    std::pair<iterator, bool> insert(const value_type& obj) {
        WRITER_LOCK(mutex_)
        return c.insert(obj);
    }

    template <class P>
    std::pair<iterator, bool> insert(P&& obj) {
        WRITER_LOCK(mutex_)
        return c.insert(obj);
    }

    iterator insert(const_iterator hint, const value_type& obj) {
        WRITER_LOCK(mutex_)
        return c.insert(hint, obj);
    }

    template <class P>
    iterator insert(const_iterator hint, P&& obj) {
        WRITER_LOCK(mutex_)
        return c.insert(hint, obj);
    }

    template <class InputIterator>
    void insert(InputIterator first, InputIterator last) {
        WRITER_LOCK(mutex_)
        c.insert(first, last);
    }

    void insert(std::initializer_list<value_type> l) {
        WRITER_LOCK(mutex_)
        c.insert(l);
    }

    iterator erase(const_iterator position) {
        WRITER_LOCK(mutex_)
        return c.erase(position);
    }

    iterator erase(const_iterator first, const_iterator last) {
        WRITER_LOCK(mutex_)
        return c.erase(first, last);
    }

    void clear() {
        WRITER_LOCK(mutex_)
        c.clear();
    }

    void merge(container_type&& source) {
        WRITER_LOCK(mutex_)
        c.merge(source);
    }

    void swap(ConcurrentContainer& m) {
        WRITER_LOCK(mutex_)
        c.swap(m.c);
    }

    void reserve(size_type n) {
        WRITER_LOCK(mutex_)
        c.reserve(n);
    }

protected:
    mutable std::shared_mutex mutex_;
    container_type c;
};

template <typename K, typename V>
class ConcurrentHashMap : public ConcurrentContainer<std::unordered_map<K, V>> {
public:
    typedef ConcurrentContainer<std::unordered_map<K, V>> base;
    typedef typename base::container_type::mapped_type mapped_type;
    typedef typename base::container_type::value_type value_type;
    typedef typename base::container_type::key_type key_type;
    typedef typename base::container_type::node_type node_type;
    typedef typename base::container_type::iterator iterator;
    typedef typename base::container_type::const_iterator const_iterator;
    typedef typename base::container_type::size_type size_type;

    using typename base::ConcurrentContainer;

    mapped_type& at(const key_type& k) {
        READER_LOCK(base::mutex_)
        return base::c.at(k);
    }

    const mapped_type& at(const key_type& k) const {
        READER_LOCK(base::mutex_)
        return base::c.at(k);
    }

    std::pair<iterator, bool> insert_or_assign(const key_type& k, mapped_type&& obj) {
        WRITER_LOCK(base::mutex_)
        return base::c.insert_or_assign(k, obj);
    }

    std::pair<iterator, bool> insert_or_assign(key_type&& k, mapped_type&& obj) {
        WRITER_LOCK(base::mutex_)
        return base::c.insert_or_assign(k, obj);
    }

    using base::insert;

    node_type insert(node_type&& nh) {
        WRITER_LOCK(base::mutex_)
        return base::c.insert(nh);
    }

    iterator insert(const_iterator hint, node_type&& nh) {
        WRITER_LOCK(base::mutex_)
        return base::c.insert(hint, nh);
    }

    using base::erase;

    size_type erase(const key_type& k) {
        WRITER_LOCK(base::mutex_)
        return base::c.erase(k);
    }

    iterator find(const key_type& k) {
        READER_LOCK(base::mutex_)
        return base::c.find(k);
    }

    const_iterator find(const key_type& k) const {
        READER_LOCK(base::mutex_)
        return base::c.find(k);
    }

    size_type count(const key_type& k) const {
        READER_LOCK(base::mutex_)
        return base::c.count(k);
    }

    bool contains(const key_type& k) const {
        READER_LOCK(base::mutex_)
        return base::c.find(k) != base::c.end();
    }

    std::vector<key_type> key_set() const {
        READER_LOCK(base::mutex_)
        std::vector<key_type> keys;

        if (base::c.empty()) {
            return keys;
        }

        keys.reserve(base::c.size());
        std::transform(base::c.begin(), base::c.end(), std::back_inserter(keys), key_selector);
        return keys;
    }

    std::vector<mapped_type> value_set() const {
        READER_LOCK(base::mutex_)
        std::vector<mapped_type> values;

        if (base::c.empty()) {
            return values;
        }

        values.reserve(base::c.size());
        std::transform(base::c.begin(), base::c.end(), std::back_inserter(values), value_selector);
        return values;
    }

    std::optional<V> random_value() const {
        READER_LOCK(base::mutex_)

        if (base::c.empty()) {
            return {};
        }

        const_iterator it = base::c.begin();
        for (size_t i = 0; i < rand() % base::c.size(); ++i) {
            ++it;
        }

        return it->second;
    }

private:
    std::function<key_type(value_type)> key_selector      = [](auto pair) { return pair.first; };
    std::function<mapped_type(value_type)> value_selector = [](auto pair) { return pair.second; };
};

template <typename K>
class ConcurrentHashSet : public ConcurrentContainer<std::unordered_set<K>> {
public:
    typedef ConcurrentContainer<std::unordered_set<K>> base;
    typedef typename base::container_type::key_type key_type;
    typedef typename base::container_type::size_type size_type;
    using typename base::ConcurrentContainer;

    bool contains(const key_type& k) const {
        READER_LOCK(base::mutex_)
        return base::c.find(k) != base::c.end();
    }

    using base::erase;
    size_type erase(const key_type& k) {
        WRITER_LOCK(base::mutex_)
        return base::c.erase(k);
    }
};

template <typename T>
class ConcurrentQueue : public ConcurrentContainer<std::deque<T>> {
public:
    using base                   = ConcurrentContainer<std::deque<T>>;
    using container_type         = typename base::container_type;
    using size_type              = typename base::size_type;
    using iterator               = typename std::deque<T>::iterator;
    using const_iterator         = typename std::deque<T>::const_iterator;
    using reverse_iterator       = typename container_type::reverse_iterator;
    using const_reverse_iterator = typename container_type::const_reverse_iterator;
    using value_type             = T;
    using reference              = value_type&;
    using const_reference        = const value_type&;

    using typename base::ConcurrentContainer;
    // ctor and dtor
    ConcurrentQueue() = default;

    // element access
    reference front() {
        READER_LOCK(base::mutex_)
        return base::c.front();
    }
    const_reference front() const {
        READER_LOCK(base::mutex_)
        return base::c.front();
    }
    reference back() {
        READER_LOCK(base::mutex_)
        return base::c.back();
    }
    const_reference back() const {
        READER_LOCK(base::mutex_)
        return base::c.back();
    }

    const_reference operator[](size_type pos) const {
        READER_LOCK(base::mutex_)
        return base::c[pos];
    }

    // reverse iterators
    reverse_iterator rbegin() noexcept {
        READER_LOCK(base::mutex_)
        return base::c.rbegin();
    }
    const_reverse_iterator rbegin() const noexcept {
        READER_LOCK(base::mutex_)
        return base::c.crbegin();
    }
    const_reverse_iterator crbegin() const noexcept {
        READER_LOCK(base::mutex_)
        return base::c.crbegin();
    }
    reverse_iterator rend() noexcept {
        READER_LOCK(base::mutex_)
        return base::c.rend();
    }
    const_reverse_iterator rend() const noexcept {
        READER_LOCK(base::mutex_)
        return base::c.crend();
    }
    const_reverse_iterator crend() const noexcept {
        READER_LOCK(base::mutex_)
        return base::c.crend();
    }

    // capacity
    size_type max_size() const noexcept {
        READER_LOCK(base::mutex_)
        return base::c.max_size();
    }
    void shrink_to_fit() {
        WRITER_LOCK(base::mutex_)
        base::c.shrink_to_fit();
    } // TODO: check what lock should I use

    // modifiers
    void push_back(const T& t) {
        WRITER_LOCK(base::mutex_)
        base::c.push_back(t);
    }
    void push_back(T&& t) {
        WRITER_LOCK(base::mutex_)
        base::c.push_back(std::move(t));
    }
    template <class... Args>
    reference emplace_back(Args&&... args) {
        WRITER_LOCK(base::mutex_)
        return base::c.emplace_back(std::forward<Args>(args)...);
    }
    void pop_front() {
        WRITER_LOCK(base::mutex_)
        base::c.pop_front();
    }

    // Hide invalid functions from base class
    template <class... Args>
    std::pair<iterator, bool> emplace(Args&&... args)       = delete;
    std::pair<iterator, bool> insert(const value_type& obj) = delete;
    template <class P>
    std::pair<iterator, bool> insert(P&& obj) = delete;
    void merge(container_type&& source)       = delete;
    void reserve(size_type n)                 = delete;
};

#endif // __SRC_UTILS_CONCURRENT_HASHMAP_H__
