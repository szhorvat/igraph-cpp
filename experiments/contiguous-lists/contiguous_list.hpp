// Prototype "design A": the wrapper class is layout-identical to the igraph struct,
// so a list's storage can be viewed as an array of wrapper objects. Iterators are
// plain pointers and references are real C++ references: no proxies.
#pragma once
#include <igraph.h>
#include <algorithm>
#include <initializer_list>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace proto {

inline void check(igraph_error_t e) {
    if (e != IGRAPH_SUCCESS) throw std::runtime_error(igraph_strerror(e));
}

class IntVec {
    igraph_vector_int_t vec;

    friend class IntVecList;
    struct adopt_tag {};
    IntVec(adopt_tag, const igraph_vector_int_t &v) noexcept : vec(v) { }

public:
    using value_type = igraph_int_t;
    using size_type = igraph_int_t;
    using difference_type = igraph_int_t;
    using reference = value_type &;
    using const_reference = const value_type &;
    using iterator = value_type *;
    using const_iterator = const value_type *;

    IntVec() { check(igraph_vector_int_init(&vec, 0)); }
    explicit IntVec(size_type n) { check(igraph_vector_int_init(&vec, n)); }
    IntVec(std::initializer_list<value_type> l) {
        check(igraph_vector_int_init_array(&vec, l.begin(), (igraph_int_t) l.size()));
    }

    // Deep copy.
    IntVec(const IntVec &o) { check(igraph_vector_int_init_copy(&vec, &o.vec)); }

    // Steal the storage; leave 'o' as a valid empty vector. Every IntVec object, including
    // a moved-from one, is always a valid igraph vector. This is what makes it safe to keep
    // IntVec objects inside igraph-owned storage.
    IntVec(IntVec &&o) {
        igraph_vector_int_t fresh;
        check(igraph_vector_int_init(&fresh, 0));
        vec = o.vec;
        o.vec = fresh;
    }

    IntVec &operator=(const IntVec &o) {
        check(igraph_vector_int_update(&vec, &o.vec));
        return *this;
    }

    // Swap-based: no allocation, never leaves either side invalid.
    IntVec &operator=(IntVec &&o) noexcept {
        std::swap(vec, o.vec);
        return *this;
    }

    ~IntVec() { igraph_vector_int_destroy(&vec); }

    friend void swap(IntVec &a, IntVec &b) noexcept { std::swap(a.vec, b.vec); }

    // View an existing igraph vector as an IntVec, without copying or taking ownership.
    static IntVec &view(igraph_vector_int_t &v) noexcept { return reinterpret_cast<IntVec &>(v); }
    static const IntVec &view(const igraph_vector_int_t &v) noexcept { return reinterpret_cast<const IntVec &>(v); }

    igraph_vector_int_t *get() noexcept { return &vec; }
    const igraph_vector_int_t *get() const noexcept { return &vec; }

    iterator begin() noexcept { return vec.stor_begin; }
    iterator end() noexcept { return vec.end; }
    const_iterator begin() const noexcept { return vec.stor_begin; }
    const_iterator end() const noexcept { return vec.end; }
    size_type size() const noexcept { return vec.end - vec.stor_begin; }
    bool empty() const noexcept { return size() == 0; }
    reference operator[](size_type i) noexcept { return vec.stor_begin[i]; }
    const_reference operator[](size_type i) const noexcept { return vec.stor_begin[i]; }
    void push_back(value_type x) { check(igraph_vector_int_push_back(&vec, x)); }

    friend bool operator==(const IntVec &a, const IntVec &b) {
        return std::equal(a.begin(), a.end(), b.begin(), b.end());
    }
    friend bool operator!=(const IntVec &a, const IntVec &b) { return !(a == b); }
    friend bool operator<(const IntVec &a, const IntVec &b) {
        return std::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end());
    }
    friend bool operator>(const IntVec &a, const IntVec &b) { return b < a; }
    friend bool operator<=(const IntVec &a, const IntVec &b) { return !(b < a); }
    friend bool operator>=(const IntVec &a, const IntVec &b) { return !(a < b); }
};

static_assert(std::is_standard_layout<IntVec>::value, "IntVec must be standard-layout");
static_assert(sizeof(IntVec) == sizeof(igraph_vector_int_t), "IntVec must be layout-identical to igraph_vector_int_t");
static_assert(alignof(IntVec) == alignof(igraph_vector_int_t), "IntVec must be layout-identical to igraph_vector_int_t");

class IntVecList {
    igraph_vector_int_list_t list;

public:
    using value_type = IntVec;
    using size_type = igraph_int_t;
    using difference_type = igraph_int_t;
    using reference = IntVec &;
    using const_reference = const IntVec &;
    using iterator = IntVec *;
    using const_iterator = const IntVec *;

    explicit IntVecList(size_type n = 0) { check(igraph_vector_int_list_init(&list, n)); }
    IntVecList(std::initializer_list<IntVec> l) : IntVecList() {
        reserve((size_type) l.size());
        for (const auto &v : l) push_back(v);
    }
    IntVecList(const IntVecList &o) { check(igraph_vector_int_list_init_copy(&list, &o.list)); }
    IntVecList(IntVecList &&o) {
        igraph_vector_int_list_t fresh;
        check(igraph_vector_int_list_init(&fresh, 0));
        list = o.list;
        o.list = fresh;
    }
    IntVecList &operator=(const IntVecList &o) { IntVecList tmp(o); swap(*this, tmp); return *this; }
    IntVecList &operator=(IntVecList &&o) noexcept { std::swap(list, o.list); return *this; }
    ~IntVecList() { igraph_vector_int_list_destroy(&list); }

    friend void swap(IntVecList &a, IntVecList &b) noexcept { std::swap(a.list, b.list); }

    igraph_vector_int_list_t *get() noexcept { return &list; }
    const igraph_vector_int_list_t *get() const noexcept { return &list; }

    iterator begin() noexcept { return reinterpret_cast<IntVec *>(list.stor_begin); }
    iterator end() noexcept { return reinterpret_cast<IntVec *>(list.end); }
    const_iterator begin() const noexcept { return reinterpret_cast<const IntVec *>(list.stor_begin); }
    const_iterator end() const noexcept { return reinterpret_cast<const IntVec *>(list.end); }

    size_type size() const noexcept { return list.end - list.stor_begin; }
    bool empty() const noexcept { return size() == 0; }
    reference operator[](size_type i) noexcept { return begin()[i]; }
    const_reference operator[](size_type i) const noexcept { return begin()[i]; }
    reference back() noexcept { return end()[-1]; }

    void reserve(size_type n) { check(igraph_vector_int_list_reserve(&list, n)); }
    void resize(size_type n) { check(igraph_vector_int_list_resize(&list, n)); }
    void clear() noexcept { igraph_vector_int_list_clear(&list); }

    // STL semantics: lvalues are copied, rvalues are moved.
    void push_back(const IntVec &v) { check(igraph_vector_int_list_push_back_copy(&list, v.get())); }
    void push_back(IntVec &&v) {
        igraph_vector_int_t *p;
        check(igraph_vector_int_list_push_back_new(&list, &p));
        std::swap(*p, *v.get()); // the new empty element goes into v
    }

    IntVec pop_back() { return IntVec(IntVec::adopt_tag{}, igraph_vector_int_list_pop_back(&list)); }

    iterator erase(const_iterator first, const_iterator last) {
        const size_type from = first - begin(), to = last - begin();
        for (size_type i = to; i > from; --i) igraph_vector_int_list_discard(&list, i - 1);
        return begin() + from;
    }
    iterator erase(const_iterator pos) { return erase(pos, pos + 1); }
};

static_assert(std::is_standard_layout<IntVecList>::value, "");

} // namespace proto
