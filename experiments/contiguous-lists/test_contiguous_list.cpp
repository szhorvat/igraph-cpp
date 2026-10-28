#include "contiguous_list.hpp"
#include <cassert>
#include <iostream>
#include <list>
#include <numeric>
#include <random>
#include <vector>
#if __cplusplus >= 202002L
#include <ranges>
#endif

using namespace proto;

#define REQUIRE(c) do { if (!(c)) { std::cerr << "FAIL line " << __LINE__ << ": " #c "\n"; std::abort(); } } while (0)

static IntVecList make() {
    // Sizes 3,1,4,1,5,9,2,6 with distinct contents.
    IntVecList l;
    int k = 0;
    for (int n : {3, 1, 4, 1, 5, 9, 2, 6}) {
        IntVec v;
        for (int i = 0; i < n; i++) v.push_back(k++);
        l.push_back(std::move(v));
    }
    return l;
}

static std::vector<igraph_int_t> sizes(const IntVecList &l) {
    std::vector<igraph_int_t> s;
    for (const auto &v : l) s.push_back(v.size());
    return s;
}

static bool by_size(const IntVec &a, const IntVec &b) { return a.size() < b.size(); }

int main() {
    // --- Value semantics: no surprises ---
    {
        IntVecList l = make();
        IntVec x = l[0];         // copy
        x.push_back(100);
        REQUIRE(l[0].size() == 3);

        auto y = l[0];           // auto deduces IntVec: a copy, not an alias
        static_assert(std::is_same<decltype(y), IntVec>::value, "");
        y.push_back(100);
        REQUIRE(l[0].size() == 3);

        auto &r = l[0];          // explicit reference
        r.push_back(100);
        REQUIRE(l[0].size() == 4);

        l[1] = IntVec{7, 8};     // assignment into the list writes through
        REQUIRE((l[1] == IntVec{7, 8}));
        IntVec z{1, 2, 3};
        l[2] = std::move(z);     // move-assignment into the list writes through
        REQUIRE((l[2] == IntVec{1, 2, 3}));
        REQUIRE(z.size() >= 0);  // z still a valid vector

        IntVec w = std::move(l[3]);   // explicit move out of the list
        REQUIRE(w.size() == 1 && l[3].empty());   // l[3] remains a valid (empty) vector

        // Copying from one list into another; buffer reuse; both were crashes before.
        IntVecList l2;
        l2.push_back(l[0]);
        IntVec buf;
        for (int i = 0; i < 3; i++) { buf = IntVec{i}; l2.push_back(buf); }
        REQUIRE(l2.size() == 4 && l2[3][0] == 2 && l2[1][0] == 0);

        const IntVecList &cl = l;
        static_assert(std::is_same<decltype(cl[0]), const IntVec &>::value, "");
    }

    // --- Legacy (C++17) algorithms ---
    {
        auto l = make();
        std::sort(l.begin(), l.end(), by_size);
        REQUIRE(std::is_sorted(l.begin(), l.end(), by_size));
        REQUIRE((sizes(l) == std::vector<igraph_int_t>{1, 1, 2, 3, 4, 5, 6, 9}));
    }
    {
        auto l = make();
        std::stable_sort(l.begin(), l.end(), by_size);
        REQUIRE((sizes(l) == std::vector<igraph_int_t>{1, 1, 2, 3, 4, 5, 6, 9}));
        REQUIRE(l[0][0] == 3 && l[1][0] == 8);   // stability: original order of the two size-1 vectors
    }
    {
        auto l = make();
        std::sort(l.begin(), l.end());             // operator< : lexicographic
        REQUIRE(std::is_sorted(l.begin(), l.end()));
        std::reverse(l.begin(), l.end());
        std::rotate(l.begin(), l.begin() + 3, l.end());
        std::nth_element(l.begin(), l.begin() + 4, l.end(), by_size);
        std::partial_sort(l.begin(), l.begin() + 3, l.end(), by_size);
        std::make_heap(l.begin(), l.end(), by_size); std::sort_heap(l.begin(), l.end(), by_size);
        REQUIRE((sizes(l) == std::vector<igraph_int_t>{1, 1, 2, 3, 4, 5, 6, 9}));
        std::shuffle(l.begin(), l.end(), std::mt19937(1));
        std::stable_partition(l.begin(), l.end(), [](const IntVec &v) { return v.size() % 2 == 0; });
        std::swap(l[0], l[1]);
        std::iter_swap(l.begin(), l.begin() + 2);
        IntVec total = std::accumulate(l.begin(), l.end(), IntVec{},
            [](IntVec acc, const IntVec &v) { for (auto x : v) acc.push_back(x); return acc; });
        REQUIRE(total.size() == 31);
    }
    {
        // remove_if + erase idiom; the elements in the "removed" tail must stay valid.
        auto l = make();
        auto it = std::remove_if(l.begin(), l.end(), [](const IntVec &v) { return v.size() == 1; });
        for (auto j = it; j != l.end(); ++j) REQUIRE(j->size() >= 0);
        l.erase(it, l.end());
        REQUIRE((sizes(l) == std::vector<igraph_int_t>{3, 4, 5, 9, 2, 6}));
        auto u = std::unique(l.begin(), l.end(), [](const IntVec &a, const IntVec &b) { return a.size() + 1 == b.size(); });
        l.erase(u, l.end());
        std::vector<IntVec> sv(l.begin(), l.end());     // copy out into a std::vector
        std::copy(sv.rbegin(), sv.rend(), l.begin());   // copy back in
        std::transform(l.begin(), l.end(), l.begin(), [](IntVec v) { v.push_back(-1); return v; });
        REQUIRE(l[0].size() >= 1 && l[0][l[0].size() - 1] == -1);
    }

    // --- igraph functions still see a valid list ---
    {
        auto l = make();
        std::sort(l.begin(), l.end(), by_size);
        IntVec moved = std::move(l[5]);
        auto rm = std::remove_if(l.begin(), l.end(), [](const IntVec &v) { return v.size() == 2; });
        (void) rm;  // do not erase: tail elements must still be valid igraph vectors
        igraph_vector_int_list_t copy;
        check(igraph_vector_int_list_init_copy(&copy, l.get()));
        igraph_vector_int_list_sort(&copy, &igraph_vector_int_lex_cmp);
        igraph_vector_int_list_destroy(&copy);
    }

    // --- Lists produced by igraph itself ---
    {
        igraph_t g;
        igraph_rng_seed(igraph_rng_default(), 42);
        check(igraph_erdos_renyi_game_gnp(&g, 30, 0.3, IGRAPH_UNDIRECTED, IGRAPH_SIMPLE_SW, IGRAPH_EDGE_UNLABELED));
        IntVecList cliques;
        check(igraph_maximal_cliques(&g, cliques.get(), 0, 0, 0));
        std::sort(cliques.begin(), cliques.end(), [](const IntVec &a, const IntVec &b) { return a.size() > b.size(); });
        REQUIRE(std::is_sorted(cliques.begin(), cliques.end(), [](const IntVec &a, const IntVec &b) { return a.size() > b.size(); }));
        for (auto &c : cliques) std::sort(c.begin(), c.end());
        igraph_destroy(&g);
    }

    // --- Viewing raw igraph data without copying ---
    {
        igraph_vector_int_t raw;
        igraph_vector_int_init_range(&raw, 0, 5);
        IntVec &v = IntVec::view(raw);
        std::reverse(v.begin(), v.end());
        REQUIRE(VECTOR(raw)[0] == 4);
        igraph_vector_int_destroy(&raw);
    }

#if __cplusplus >= 202002L
    // --- C++20 concepts and ranges ---
    static_assert(std::contiguous_iterator<IntVecList::iterator>);
    static_assert(std::ranges::contiguous_range<IntVecList>);
    static_assert(std::ranges::random_access_range<const IntVecList>);
    static_assert(std::sortable<IntVecList::iterator>);
    static_assert(std::permutable<IntVecList::iterator>);
    static_assert(std::regular<IntVec>);
    {
        auto l = make();
        std::ranges::sort(l, {}, &IntVec::size);
        REQUIRE((sizes(l) == std::vector<igraph_int_t>{1, 1, 2, 3, 4, 5, 6, 9}));
        std::ranges::stable_sort(l, std::ranges::greater{}, &IntVec::size);
        std::ranges::reverse(l);
        auto [b, e] = std::ranges::remove_if(l, [](const IntVec &v) { return v.size() > 4; });
        l.erase(b, e);
        REQUIRE((sizes(l) == std::vector<igraph_int_t>{1, 1, 2, 3, 4}));
        igraph_int_t total = 0;
        for (auto n : l | std::views::filter([](const IntVec &v) { return v.size() > 1; })
                        | std::views::transform(&IntVec::size))
            total += n;
        REQUIRE(total == 9);
        for (auto &v : l | std::views::reverse) v.push_back(0);
        REQUIRE(l[0].size() == 2);
    }
#endif

    std::cout << "all passed (C++" << __cplusplus / 100 % 100 << ")\n";
    return 0;
}
