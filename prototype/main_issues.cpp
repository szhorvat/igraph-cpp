// Demonstrates behavioural problems of the list wrappers on igraph-cpp main.
// Build against the headers in ../include (with igraph 1.x: drop the version #error
// in igraph.hpp and pass the extra arguments to igraph_is_simple/igraph_edges), e.g.
//   g++ -std=c++17 -fsanitize=address,undefined -I../include $(pkg-config --cflags --libs igraph) main_issues.cpp
// Every line marked "(bug)" in the output is a silent misbehaviour; see README.md.
#include <algorithm>
#include <iostream>
#include <iterator>
#include <type_traits>

using namespace ig;

static void print(const char *label, const IntVecList &list) {
    std::cout << label << ": ";
    for (const auto &v : list) {
        std::cout << '(';
        for (auto x : v) std::cout << x << ' ';
        std::cout << ") ";
    }
    std::cout << '\n';
}

int main() {
    igraph_set_error_handler(igraph_error_handler_printignore);

    IntVecList list;
    list.push_back(IntVec{1, 2, 3});
    list.push_back(IntVec{4, 5});
    list.push_back(IntVec{6});
    print("initial", list);

    // H2: an explicitly typed "copy" is not a copy (guaranteed copy elision of the alias prvalue).
    {
        IntVec x = list[0];
        x.push_back(42);
        std::cout << "H2  'IntVec x = list[0]; x.push_back(42)' changed list[0]: "
                  << (list[0].size() == 4 ? "YES (bug)" : "no") << '\n';
        list[0].pop_back();
    }

    // H4: move-assigning into an element silently does nothing to the list and destroys the source.
    {
        IntVec v{7, 8, 9};
        list[2] = std::move(v);
        std::cout << "H4  'list[2] = std::move(v)' updated list[2]: "
                  << (list[2].size() == 3 ? "yes" : "NO (bug: silently dropped)") << '\n';
    }

    // H7: move assignment into an lvalue silently deep copies.
    {
        IntVec a{1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
        IntVec b;
        const igraph_int_t *old_data = a.data();
        b = std::move(a);
        std::cout << "H7  'b = std::move(a)' moved the buffer: "
                  << (b.data() == old_data ? "yes" : "NO (bug: deep copy, a still intact with size "
                                                     + std::to_string(a.size()) + ")") << '\n';
    }

    // H5: std::sort is a silent no-op (insertion sort moves through aliasing temporaries).
    {
        IntVecList l;
        l.push_back(IntVec{1, 2, 3});
        l.push_back(IntVec{4});
        l.push_back(IntVec{5, 6});
        std::sort(l.begin(), l.end(), [](const IntVec &a, const IntVec &b) { return a.size() < b.size(); });
        print("H5  after std::sort by size", l);
        std::cout << "H5  sorted: " << (std::is_sorted(l.begin(), l.end(),
                 [](const IntVec &a, const IntVec &b) { return a.size() < b.size(); }) ? "yes" : "NO (bug)") << '\n';
    }

    // H6: post-increment returns the incremented iterator.
    {
        auto it = list.begin();
        auto it2 = it++;
        std::cout << "H6  'it2 = it++' leaves it2 at begin: " << (it2 == list.begin() ? "yes" : "NO (bug)") << '\n';
    }

    // H8: const_iterator is not copyable (user-declared move ctor deletes the copy ctor).
    std::cout << "H8  const_iterator copy constructible: "
              << (std::is_copy_constructible<IntVecList::const_iterator>::value ? "yes" : "NO (bug)") << '\n';

    // H1: 'auto' copy is an alias.
    {
        auto x = list[1];
        x.push_back(99);
        std::cout << "H1  'auto x = list[1]; x.push_back(99)' changed list[1]: "
                  << (list[1].size() == 3 ? "YES (vector<bool> hazard)" : "no") << '\n';
    }

    // H3: alias outliving the storage -> use after free detected by ASan? (list reallocation)
    {
        IntVecList l(1);
        IntVec x = l[0];        // looks like a copy, is an alias
        for (int i = 0; i < 100; i++) l.push_back_new();  // reallocates
        std::cout << "H3  reading x after reallocation (ASan should complain next): " << std::flush;
        std::cout << x.size() << '\n';
    }
    return 0;
}
