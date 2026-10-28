// Prototype of the *alternative* design: a list whose iterator dereferences to a
// proxy "reference class" (like std::vector<bool>), made to satisfy the C++20
// iterator concepts. It exists to establish rigorously what the proxy approach can
// and cannot do; the recommended design is the in-place one in ../inplace.
//
// Findings demonstrated by test_proxy.cpp:
//  1. std::indirectly_writable<It, T> requires assignment through a *const rvalue*
//     proxy ('const_cast<const Ref &&>(*it) = t'). A proxy whose assignment is
//     ref-qualified '&&' (the trick used on main to stop 'auto x = v[i]; x = ...')
//     therefore fails the concept. Making the assignment 'const &&' satisfies it,
//     and still rejects assignment through a named (lvalue) proxy, so the two ideas
//     are compatible. Mutable const-qualified members in turn require a *separate*
//     const proxy type for const_iterator.
//  2. With that, std::ranges::sort and the classic algorithms work.
//  3. What cannot be fixed: 'auto x = list[i]' is an alias, not a copy, and C++17
//     guaranteed copy elision means no constructor can be deleted to prevent it.
//     Either lvalue proxies are mutable (the std::vector<bool> trap) or they are not,
//     in which case 'for (auto &&v : list) v.push_back(..)' does not compile either.
//
// Requires C++20 and igraph 1.x. Uses ig::IntVec from the in-place prototype as the
// value type (an owning vector).

#ifndef IGCPP_PROXY_HPP
#define IGCPP_PROXY_HPP

#include "../inplace/inplace.hpp"

#include <compare>
#include <concepts>
#include <iterator>

namespace ig::proxy {

// Mutable proxy to an igraph_vector_int_t stored in a list.
class VecRef {
    igraph_vector_int_t *p;

public:
    explicit VecRef(igraph_vector_int_t *p_) noexcept : p(p_) { }

    // Reading: expose the element through the owning wrapper's const interface.
    const IntVec & vec() const noexcept { return IntVec::alias(*p); }
    operator const IntVec & () const noexcept { return vec(); }
    igraph_int_t size() const noexcept { return vec().size(); }

    // Writing. Deliberately 'const &&'-qualified:
    //  - 'const' because std::indirectly_writable assigns through 'const Ref &&';
    //  - '&&' so that assignment through a *named* proxy ('auto x = list[i]; x = v;')
    //    does not compile, while 'list[i] = v' and '*it = v' do.
    const VecRef & operator = (const IntVec &v) const && {
        check(igraph_vector_int_update(p, v.get()));
        return *this;
    }
    const VecRef & operator = (IntVec &&v) const && noexcept {
        igraph_vector_int_swap(p, v.get());
        return *this;
    }
    // Assigning one element to another.
    const VecRef & operator = (const VecRef &other) const && {
        check(igraph_vector_int_update(p, other.p));
        return *this;
    }

    // Mutating members. Not ref-qualified here, to show the std::vector<bool> trap in
    // the tests; qualifying them '&&' closes the trap but breaks 'for (auto &&v : list)'.
    void push_back(igraph_int_t e) const { check(igraph_vector_int_push_back(p, e)); }

    // Customisation points used by the ranges algorithms.
    friend IntVec iter_move(const VecRef &r) noexcept {
        IntVec out;
        igraph_vector_int_swap(out.get(), r.p);
        return out;
    }
    friend void iter_swap(const VecRef &a, const VecRef &b) noexcept {
        igraph_vector_int_swap(a.p, b.p);
    }
    friend void swap(const VecRef &a, const VecRef &b) noexcept {   // for std::iter_swap / std::swap(*i, *j)
        igraph_vector_int_swap(a.p, b.p);
    }
    friend bool operator == (const VecRef &a, const VecRef &b) noexcept { return a.vec() == b.vec(); }
};

// Variant with a plain '&&' assignment (not const), as one would write at first.
// It does NOT satisfy std::indirectly_writable; see test_proxy.cpp.
class VecRefNonConstAssign {
    igraph_vector_int_t *p;
public:
    explicit VecRefNonConstAssign(igraph_vector_int_t *p_) noexcept : p(p_) { }
    operator const IntVec & () const noexcept { return IntVec::alias(*p); }
    VecRefNonConstAssign & operator = (const IntVec &v) && { check(igraph_vector_int_update(p, v.get())); return *this; }
    VecRefNonConstAssign & operator = (IntVec &&v) && noexcept { igraph_vector_int_swap(p, v.get()); return *this; }
};

template<typename Ref>
class Iterator {
    igraph_vector_int_t *p = nullptr;

public:
    using value_type = IntVec;
    using reference = Ref;
    using difference_type = igraph_int_t;
    using iterator_category = std::random_access_iterator_tag;   // for pre-C++20 algorithms
    using iterator_concept = std::random_access_iterator_tag;

    Iterator() = default;
    explicit Iterator(igraph_vector_int_t *p_) noexcept : p(p_) { }

    Ref operator * () const noexcept { return Ref(p); }
    Ref operator [] (difference_type n) const noexcept { return Ref(p + n); }

    Iterator & operator ++ () noexcept { ++p; return *this; }
    Iterator operator ++ (int) noexcept { Iterator t = *this; ++p; return t; }
    Iterator & operator -- () noexcept { --p; return *this; }
    Iterator operator -- (int) noexcept { Iterator t = *this; --p; return t; }
    Iterator & operator += (difference_type n) noexcept { p += n; return *this; }
    Iterator & operator -= (difference_type n) noexcept { p -= n; return *this; }

    friend Iterator operator + (Iterator i, difference_type n) noexcept { return i += n; }
    friend Iterator operator + (difference_type n, Iterator i) noexcept { return i += n; }
    friend Iterator operator - (Iterator i, difference_type n) noexcept { return i -= n; }
    friend difference_type operator - (Iterator a, Iterator b) noexcept { return a.p - b.p; }

    friend bool operator == (Iterator a, Iterator b) noexcept = default;
    friend auto operator <=> (Iterator a, Iterator b) noexcept = default;

    // iter_move / iter_swap are found by ADL on the reference type (VecRef) via *it.
    friend IntVec iter_move(const Iterator &i) noexcept { return iter_move(*i); }
    friend void iter_swap(const Iterator &a, const Iterator &b) noexcept { iter_swap(*a, *b); }
};

class ProxyList {
    igraph_vector_int_list_t list;

public:
    using value_type = IntVec;
    using reference = VecRef;
    using iterator = Iterator<VecRef>;

    ProxyList() { check(igraph_vector_int_list_init(&list, 0)); }
    ProxyList(const ProxyList &) = delete;
    ProxyList & operator = (const ProxyList &) = delete;
    ~ProxyList() { igraph_vector_int_list_destroy(&list); }

    operator igraph_vector_int_list_t * () noexcept { return &list; }

    igraph_int_t size() const noexcept { return list.end - list.stor_begin; }
    iterator begin() noexcept { return iterator(list.stor_begin); }
    iterator end() noexcept { return iterator(list.end); }
    VecRef operator [] (igraph_int_t i) noexcept { return VecRef(list.stor_begin + i); }

    void push_back(const IntVec &v) { check(igraph_vector_int_list_push_back_copy(&list, v.get())); }
};

} // namespace ig::proxy

#endif // IGCPP_PROXY_HPP
