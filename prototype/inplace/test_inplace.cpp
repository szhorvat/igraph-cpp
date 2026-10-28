// Tests for the in-place list prototype. Compile with -std=c++17 or -std=c++20;
// the C++20 ranges checks are enabled automatically when available.

#include "inplace.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <numeric>
#include <vector>

#if __cplusplus >= 202002L && __has_include(<ranges>)
#include <ranges>
#if defined(__cpp_lib_ranges) && __cpp_lib_ranges >= 201911L
#define HAVE_RANGES 1
#endif
#endif

using namespace ig;

static int failures = 0;

#define EXPECT(cond) do { if (!(cond)) { std::printf("FAILED: %s (line %d)\n", #cond, __LINE__); ++failures; } } while (0)

static bool by_size(const IntVec &a, const IntVec &b) { return a.size() < b.size(); }

static IntVecList make_list() {
    IntVecList list;
    list.push_back(IntVec{1, 2, 3});
    list.push_back(IntVec{4});
    list.push_back(IntVec{5, 6});
    list.push_back(IntVec{7, 8, 9, 10});
    list.push_back(IntVec{});
    return list;
}

static void test_vec_basics() {
    IntVec v{3, 1, 2};
    EXPECT(v.size() == 3);
    std::sort(v.begin(), v.end());
    EXPECT(v[0] == 1 && v[2] == 3);

    // Copy is deep.
    IntVec c = v;
    c.push_back(4);
    EXPECT(v.size() == 3 && c.size() == 4);

    // Move leaves a valid empty vector behind; the buffer is transferred.
    const igraph_int_t *data = c.data();
    IntVec m = std::move(c);
    EXPECT(m.data() == data && m.size() == 4);
    EXPECT(c.size() == 0);
    c.push_back(1);   // moved-from object is usable
    EXPECT(c.size() == 1);

    // Move assignment transfers the buffer.
    IntVec t;
    t = std::move(m);
    EXPECT(t.data() == data && t.size() == 4);
    EXPECT(m.size() == 0);
    m = t;   // copy-assign into moved-from
    EXPECT(m == t);

    // Capture takes ownership of a raw igraph vector.
    igraph_vector_int_t raw;
    igraph_vector_int_init_range(&raw, 0, 5);
    IntVec captured{Capture(raw)};
    EXPECT(captured.size() == 5 && captured[4] == 4);

    // alias() gives the wrapper interface to an externally owned vector (here a view
    // over a stack array that must never be freed).
    igraph_int_t buf[] = {10, 20, 30};
    igraph_vector_int_t view = igraph_vector_int_view(buf, 3);
    const IntVec &av = IntVec::alias(view);
    EXPECT(av.size() == 3 && av[1] == 20);
    EXPECT(std::accumulate(av.begin(), av.end(), igraph_int_t{0}) == 60);
    IntVec::alias(view)[1] = 25;   // writes through to buf
    EXPECT(buf[1] == 25);
}

static void test_list_reference_semantics() {
    IntVecList list = make_list();

    // 'auto' makes a deep copy, exactly as with std::vector<T>.
    {
        auto x = list[0];
        x.push_back(42);
        EXPECT(list[0].size() == 3);
    }
    // Explicitly typed copy is a copy.
    {
        IntVec x = list[0];
        x.push_back(42);
        EXPECT(list[0].size() == 3);
    }
    // A reference is a reference.
    {
        IntVec &r = list[0];
        r.push_back(42);
        EXPECT(list[0].size() == 4);
        list[0].pop_back();
        static_assert(std::is_same<decltype(list[0]), IntVec &>::value, "operator[] returns a true reference");
        static_assert(std::is_same<decltype(*list.begin()), IntVec &>::value, "iterator dereferences to a true reference");
        static_assert(std::is_same<IntVecList::iterator, IntVec *>::value, "iterator is a pointer");
    }
    // Range-based for with auto& modifies elements; with auto it modifies copies.
    for (auto &v : list) v.push_back(0);
    EXPECT(list[1].size() == 2 && list[4].size() == 1);
    for (auto v : list) v.clear();
    EXPECT(list[1].size() == 2);
    for (auto &v : list) v.pop_back();

    // Assigning into an element: copy and move both write through.
    {
        IntVec src{7, 8, 9};
        list[1] = src;
        EXPECT(list[1].size() == 3 && src.size() == 3);
        const igraph_int_t *data = src.data();
        list[1] = std::move(src);
        EXPECT(list[1].data() == data);   // buffer moved into the list
        EXPECT(src.size() == 3);          // swap semantics: src holds the old element
        list[1] = IntVec{4};
    }

    // push_back by move transfers the buffer, by copy duplicates it.
    {
        IntVec src{1, 2};
        const igraph_int_t *data = src.data();
        list.push_back(std::move(src));
        EXPECT(list.back().data() == data && src.empty());
        list.push_back(list[0]);   // copy of an element of the same list (reallocation-safe since push_back_copy copies first)
        EXPECT(list.back() == list[0]);
        list.pop_back();
        list.pop_back();
    }

    // emplace_back, pop_back, take, erase, insert
    {
        IntVec &n = list.emplace_back();
        n.push_back(99);
        EXPECT(list.back().size() == 1 && list.back()[0] == 99);
        IntVec popped = list.pop_back();
        EXPECT(popped.size() == 1 && popped[0] == 99 && list.size() == 5);

        IntVec taken = list.take(list.begin() + 1);
        EXPECT(taken.size() == 1 && taken[0] == 4 && list.size() == 4);
        list.insert(list.begin() + 1, std::move(taken));
        EXPECT(list.size() == 5 && list[1].size() == 1 && list[1][0] == 4);

        auto it = list.erase(list.begin() + 1, list.begin() + 3);
        EXPECT(list.size() == 3 && it == list.begin() + 1 && (*it).size() == 4);
        list.insert(list.begin() + 1, IntVec{4});
        list.insert(list.begin() + 2, IntVec{5, 6});
        EXPECT(list == make_list());
    }

    // Copy and move of whole lists.
    {
        IntVecList copy = list;
        EXPECT(copy == list);
        copy[0].push_back(1);
        EXPECT(copy != list);
        IntVecList moved = std::move(copy);
        EXPECT(moved != list && copy.empty());
        copy = list;
        EXPECT(copy == list);
    }
}

static void test_std_algorithms() {
    IntVecList list = make_list();

    std::sort(list.begin(), list.end(), by_size);
    EXPECT(std::is_sorted(list.begin(), list.end(), by_size));
    EXPECT(list[0].empty() && list[4].size() == 4);

    std::reverse(list.begin(), list.end());
    EXPECT(list[0].size() == 4);

    std::stable_sort(list.begin(), list.end(), by_size);
    EXPECT(std::is_sorted(list.begin(), list.end(), by_size));

    std::rotate(list.begin(), list.begin() + 2, list.end());
    EXPECT(list[0].size() == 2);

    auto new_end = std::remove_if(list.begin(), list.end(), [](const IntVec &v) { return v.size() % 2 == 0; });
    list.erase(new_end, list.end());
    EXPECT(list.size() == 2 && list[0].size() == 3 && list[1].size() == 1);

    // Larger list, exercising the introsort path (> 16 elements) and heap sort.
    IntVecList big;
    for (int i = 0; i < 100; ++i) {
        big.push_back(IntVec((i * 37) % 23));
    }
    std::sort(big.begin(), big.end(), by_size);
    EXPECT(std::is_sorted(big.begin(), big.end(), by_size));
    std::make_heap(big.begin(), big.end(), by_size);
    std::sort_heap(big.begin(), big.end(), by_size);
    EXPECT(std::is_sorted(big.begin(), big.end(), by_size));
    std::partial_sort(big.begin(), big.begin() + 10, big.end(), [](const IntVec &a, const IntVec &b) { return a.size() > b.size(); });
    EXPECT(big[0].size() == 22);

    // Lists of wrappers inside standard containers.
    std::vector<IntVec> sv;
    for (int i = 0; i < 50; ++i) sv.push_back(IntVec{i});   // reallocations move elements
    EXPECT(sv[49][0] == 49);
}

static void test_graphs() {
    IntVec edges{0, 1, 1, 2, 3, 4, 5, 6};
    Graph g(edges, 9, IGRAPH_DIRECTED);
    EXPECT(g.vcount() == 9 && g.ecount() == 4);

    GraphList comps;
    check(igraph_decompose(g, comps, IGRAPH_WEAK, -1, -1));
    EXPECT(comps.size() == 5);

    std::sort(comps.begin(), comps.end(), [](const Graph &a, const Graph &b) { return a.vcount() < b.vcount(); });
    EXPECT(comps[0].vcount() == 1 && comps[1].vcount() == 1 && comps[4].vcount() == 3);
    EXPECT(comps[0] == comps[1]);   // both singletons

    Graph moved = std::move(comps[4]);
    EXPECT(moved.vcount() == 3 && comps[4].vcount() == 0);
    comps[4] = std::move(moved);
    EXPECT(comps[4].vcount() == 3);

    Graph copy = comps[4];
    EXPECT(copy == comps[4]);
    comps.push_back(std::move(copy));
    EXPECT(comps.size() == 6 && copy.vcount() == 0);
    Graph back = comps.pop_back();
    EXPECT(back.vcount() == 3);

    igraph_t raw;
    check(igraph_ring(&raw, 5, IGRAPH_UNDIRECTED, false, true));
    EXPECT(Graph::alias(raw).ecount() == 5);
    Graph owned{Capture(raw)};
    EXPECT(owned.ecount() == 5);
}

#ifdef HAVE_RANGES
static void test_ranges() {
    static_assert(std::contiguous_iterator<IntVecList::iterator>);
    static_assert(std::ranges::contiguous_range<IntVecList>);
    static_assert(std::ranges::contiguous_range<const IntVecList>);
    static_assert(std::ranges::contiguous_range<GraphList>);
    static_assert(std::sortable<IntVecList::iterator>);
    static_assert(std::indirectly_writable<IntVecList::iterator, IntVec>);
    static_assert(std::indirectly_writable<IntVecList::iterator, IntVec &&>);

    IntVecList list = make_list();
    std::ranges::sort(list, {}, &IntVec::size);   // projection
    EXPECT(std::ranges::is_sorted(list, {}, &IntVec::size));

    auto sizes = list | std::views::transform(&IntVec::size);
    EXPECT(std::ranges::equal(sizes, std::vector<igraph_int_t>{0, 1, 2, 3, 4}));

    for (IntVec &v : list | std::views::filter([](const IntVec &v) { return v.size() > 2; })) {
        v.push_back(-1);
    }
    EXPECT(list[3].size() == 4 && list[4].size() == 5 && list[2].size() == 2);

    igraph_int_t total = 0;
    for (igraph_int_t x : list | std::views::join) total += x;
    EXPECT(total == 1 + 2 + 3 + 4 + 5 + 6 + 7 + 8 + 9 + 10 - 2);

    // Moving out of a range element into a new owner; ranges::iter_move is a real move here.
    IntVec taken = std::ranges::iter_move(list.begin() + 4);
    EXPECT(taken.size() == 5 && list[4].empty());

    GraphList gl;
    gl.push_back(Graph(3));
    gl.push_back(Graph(1));
    gl.push_back(Graph(2));
    std::ranges::sort(gl, std::ranges::greater{}, &Graph::vcount);
    EXPECT(gl[0].vcount() == 3 && gl[2].vcount() == 1);
}
#endif

int main() {
    igraph_set_error_handler(igraph_error_handler_printignore);

    test_vec_basics();
    test_list_reference_semantics();
    test_std_algorithms();
    test_graphs();
#ifdef HAVE_RANGES
    test_ranges();
    std::printf("C++20 ranges checks: enabled\n");
#else
    std::printf("C++20 ranges checks: disabled\n");
#endif

    if (failures == 0) {
        std::printf("All checks passed.\n");
        return EXIT_SUCCESS;
    }
    std::printf("%d check(s) failed.\n", failures);
    return EXIT_FAILURE;
}
