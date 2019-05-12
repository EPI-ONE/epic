#ifndef __SRC_SUPPORT_ALLOCATORS_LOCKED_H__
#define __SRC_SUPPORT_ALLOCATORS_LOCKED_H__

#include "lockedpool.h"

/*
 * Allocator that locks its contents
 * from being paged  out of memory
 */
template <typename T>
struct locked_allocator : public std::allocator<T> {
    typedef std::allocator<T> base;
    typedef typename base::size_type size_type;
    typedef typename base::difference_type difference_type;
    typedef typename base::pointer pointer;
    typedef typename base::const_pointer const_pointer;
    typedef typename base::reference reference;
    typedef typename base::const_reference const_reference;
    typedef typename base::value_type value_type;

    locked_allocator() noexcept {}
    locked_allocator(const locked_allocator& a) noexcept : base(a) {}
    template <typename U>
    locked_allocator(const locked_allocator<U>& a) noexcept : base(a) {}
    ~locked_allocator() noexcept {}

    template <typename _Other>
    struct rebind {
        typedef locked_allocator<_Other> other;
    };

    T* allocate(std::size_t n, const void* hint = 0) {
        T* allocation = static_cast<T*>(LockedPoolManager::Instance().alloc(sizeof(T) * n));
        if (!allocation) {
            throw std::bad_alloc();
        }

        return allocation;
    }

    void deallocate(T* p, std::size_t n) {
        LockedPoolManager::Instance().free(p);
    }
};

#endif
