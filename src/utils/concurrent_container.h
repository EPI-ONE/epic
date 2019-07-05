#ifndef __SRC_UTILS_CONCURRENT_HASHMAP_H__
#define __SRC_UTILS_CONCURRENT_HASHMAP_H__

#include <shared_mutex>
#include <unordered_map>
#include <unordered_set>

#define READER_LOCK(mu) std::shared_lock<std::shared_mutex> reader(mu);
#define WRITER_LOCK(mu) std::unique_lock<std::shared_mutex> writer(mu);

template <typename Container>
class ConcurrentContainer {
public:
    typedef Container container_type;
    typedef typename container_type::key_type key_type;
    typedef typename container_type::value_type value_type;
    typedef typename container_type::hasher hasher;
    typedef typename container_type::key_equal key_equal;
    typedef typename container_type::allocator_type allocator_type;
    typedef typename container_type::size_type size_type;
    typedef typename container_type::difference_type difference_type;
    typedef typename container_type::node_type node_type;
    typedef typename container_type::node_type insert_return_type;
    typedef typename container_type::iterator iterator;
    typedef typename container_type::const_iterator const_iterator;
    typedef typename container_type::local_iterator local_iterator;
    typedef typename container_type::const_local_iterator const_local_iterator;
    typedef value_type& reference;
    typedef const value_type& const_reference;


    ConcurrentContainer() : mutex_(), c() {}
    ConcurrentContainer(const ConcurrentContainer& m) : mutex_(), c(m.c) {}
    ConcurrentContainer(ConcurrentContainer&& m) : mutex_(), c(std::move(m.c)) {}
    ~ConcurrentContainer() = default;

    ConcurrentContainer& operator=(const ConcurrentContainer& m) {
        WRITER_LOCK(mutex_);
        c = m.c;
        return *this;
    }

    bool empty() const {
        READER_LOCK(mutex_);
        return c.empty();
    }

    size_type size() const {
        READER_LOCK(mutex_);
        return c.size();
    }

    iterator begin() {
        READER_LOCK(mutex_);
        return c.begin();
    }

    iterator end() {
        READER_LOCK(mutex_);
        return c.end();
    }

    const_iterator begin() const {
        READER_LOCK(mutex_);
        return c.cbegin();
    }

    const_iterator end() const {
        READER_LOCK(mutex_);
        return c.cend();
    }

    template <class... Args>
    std::pair<iterator, bool> emplace(Args&&... args) {
        WRITER_LOCK(mutex_);
        return c.emplace(std::forward<Args>(args)...);
    }

    std::pair<iterator, bool> insert(const value_type& obj) {
        WRITER_LOCK(mutex_);
        return c.insert(obj);
    }

    template <class P>
    std::pair<iterator, bool> insert(P&& obj) {
        WRITER_LOCK(mutex_);
        return c.insert(obj);
    }

    iterator insert(const_iterator hint, const value_type& obj) {
        WRITER_LOCK(mutex_);
        return c.insert(hint, obj);
    }

    template <class P>
    iterator insert(const_iterator hint, P&& obj) {
        WRITER_LOCK(mutex_);
        return c.insert(hint, obj);
    }

    template <class InputIterator>
    void insert(InputIterator first, InputIterator last) {
        WRITER_LOCK(mutex_);
        c.insert(first, last);
    }

    void insert(std::initializer_list<value_type> l) {
        WRITER_LOCK(mutex_);
        c.insert(l);
    }

    node_type extract(const_iterator position) {
        WRITER_LOCK(mutex_);
        return c.extract(position);
    }

    node_type extract(const key_type& x) {
        WRITER_LOCK(mutex_);
        return c.extract(x);
    }

    insert_return_type insert(node_type&& nh) {
        WRITER_LOCK(mutex_);
        return c.insert(nh);
    }

    iterator insert(const_iterator hint, node_type&& nh) {
        WRITER_LOCK(mutex_);
        return c.insert(hint, nh);
    }

    iterator erase(const_iterator position) {
        WRITER_LOCK(mutex_);
        return c.erase(position);
    }

    size_type erase(const key_type& k) {
        WRITER_LOCK(mutex_);
        return c.erase(k);
    }

    iterator erase(const_iterator first, const_iterator last) {
        WRITER_LOCK(mutex_);
        return c.erase(first, last);
    }

    void clear() {
        WRITER_LOCK(mutex_);
        c.clear();
    }

    void merge(ConcurrentContainer<container_type>& source) {
        WRITER_LOCK(mutex_);
        c.merge(source.c);
    }

    void merge(container_type& source) {
        WRITER_LOCK(mutex_);
        c.merge(source);
    }

    void merge(container_type&& source) {
        WRITER_LOCK(mutex_);
        c.merge(source);
    }

    void swap(ConcurrentContainer& m) {
        WRITER_LOCK(mutex_);
        c.swap(m.c);
    }

    iterator find(const key_type& k) {
        READER_LOCK(mutex_);
        return c.find(k);
    }

    const_iterator find(const key_type& k) const {
        READER_LOCK(mutex_);
        return c.find(k);
    }

    size_type count(const key_type& k) const {
        READER_LOCK(mutex_);
        return c.count(k);
    }

    void reserve(size_type n) {
        WRITER_LOCK(mutex_);
        c.reserve(n);
    }

    bool contains(const key_type& k) const {
        READER_LOCK(mutex_);
        return c.find(k) != c.end();
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
    typedef typename base::container_type::key_type key_type;
    typedef typename base::container_type::iterator iterator;
    typedef typename base::container_type::const_iterator const_iterator;

    using typename base::ConcurrentContainer;

    mapped_type& at(const key_type& k) {
        READER_LOCK(base::mutex_);
        return base::c.at(k);
    }

    const mapped_type& at(const key_type& k) const {
        READER_LOCK(base::mutex_);
        return base::c.at(k);
    }

    std::pair<iterator, bool> insert_or_assign(const key_type& k, mapped_type&& obj) {
        WRITER_LOCK(base::mutex_);
        return base::c.insert_or_assign(k, obj);
    }

    std::pair<iterator, bool> insert_or_assign(key_type&& k, mapped_type&& obj) {
        WRITER_LOCK(base::mutex_);
        return base::c.insert_or_assign(k, obj);
    }

    using base::erase;

    iterator erase(iterator position) {
        WRITER_LOCK(base::mutex_);
        return base::c.erase(position);
    }
};

template <typename K>
class ConcurrentHashSet : public ConcurrentContainer<std::unordered_set<K>> {
public:
    using ConcurrentContainer<std::unordered_set<K>>::ConcurrentContainer;
};

#endif // __SRC_UTILS_CONCURRENT_HASHMAP_H__
