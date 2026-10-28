// See proxy.hpp for what this test establishes. Requires C++20.
#include "proxy.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <ranges>

using namespace ig;
using namespace ig::proxy;

static int failures = 0;
#define EXPECT(cond) do { if (!(cond)) { std::printf("FAILED: %s (line %d)\n", #cond, __LINE__); ++failures; } } while (0)

// --- Finding 1: the concept requires assignment through a const rvalue proxy. ---

static_assert(std::random_access_iterator<Iterator<VecRef>>);
static_assert(std::indirectly_writable<Iterator<VecRef>, IntVec>);
static_assert(std::indirectly_writable<Iterator<VecRef>, IntVec &&>);
static_assert(std::indirectly_writable<Iterator<VecRef>, const IntVec &>);
static_assert(std::indirectly_movable_storable<Iterator<VecRef>, Iterator<VecRef>>);
static_assert(std::indirectly_swappable<Iterator<VecRef>>);
using SizeCompare = bool (*)(const IntVec &, const IntVec &);
static_assert(std::sortable<Iterator<VecRef>, SizeCompare>);
static_assert(std::ranges::random_access_range<ProxyList>);

// The same proxy with a non-const '&&' assignment is a random access iterator but is
// not writable in the sense of the ranges library, so std::ranges::sort rejects it.
static_assert(std::random_access_iterator<Iterator<VecRefNonConstAssign>>);
static_assert(!std::indirectly_writable<Iterator<VecRefNonConstAssign>, IntVec>);
static_assert(!std::sortable<Iterator<VecRefNonConstAssign>, SizeCompare>);

// --- Finding 3: what 'const &&' buys and what it cannot. ---

// Assignment through a named proxy does not compile ...
template<typename R> concept lvalue_assignable = requires(R r, IntVec v) { r = v; };
static_assert(!lvalue_assignable<VecRef>);
// ... but assignment through the temporary returned by [] or * does.
template<typename L> concept element_assignable = requires(L l, IntVec v) { l[0] = v; *l.begin() = v; };
static_assert(element_assignable<ProxyList>);
// Mutating members, however, cannot be ref-qualified without also breaking
// 'for (auto &&v : list) v.push_back(1)', because 'v' is an lvalue there.
template<typename R> concept lvalue_mutable = requires(R r) { r.push_back(1); };
static_assert(lvalue_mutable<VecRef>);

static bool by_size(const IntVec &a, const IntVec &b) { return a.size() < b.size(); }

int main() {
    igraph_set_error_handler(igraph_error_handler_printignore);

    ProxyList list;
    list.push_back(IntVec{1, 2, 3});
    list.push_back(IntVec{4});
    list.push_back(IntVec{5, 6});
    list.push_back(IntVec{7, 8, 9, 10});
    list.push_back(IntVec{});

    // --- Finding 2: algorithms work. ---
    std::ranges::sort(list, by_size);
    EXPECT(std::ranges::is_sorted(list, by_size));
    EXPECT(list[0].size() == 0 && list[4].size() == 4);

    std::ranges::reverse(list);
    std::sort(list.begin(), list.end(), by_size);   // classic algorithm, insertion-sort path
    EXPECT(std::is_sorted(list.begin(), list.end(), by_size));

    list[1] = IntVec{40, 41};                        // assignment through the proxy writes through
    EXPECT(list[1].size() == 2 && list[1].vec()[1] == 41);
    *list.begin() = list[1];                         // element to element
    EXPECT(list[0].vec() == list[1].vec());

    IntVec copy = list[4];                           // explicit type: a real copy (converting ctor)
    copy.push_back(0);
    EXPECT(list[4].size() == 4);

    // --- Finding 3: the trap that remains. ---
    auto x = list[4];                                // looks like a copy ...
    x.push_back(0);                                  // ... but writes into the list
    EXPECT(list[4].size() == 5);                     // (this is the std::vector<bool> hazard)
    // auto &r = list[4];                            // does not compile (good) ...
    for (auto &&v : list) v.push_back(1);            // ... so this is the only way to mutate in a loop,
    EXPECT(list[0].size() == 3);                     // and it needs the same lvalue mutability that
                                                     // makes 'auto x = list[i]' a trap.
    if (failures == 0) {
        std::printf("All checks passed.\n");
        return EXIT_SUCCESS;
    }
    std::printf("%d check(s) failed.\n", failures);
    return EXIT_FAILURE;
}
