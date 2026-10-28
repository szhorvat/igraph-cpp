# List wrapper prototypes

Experimental code supporting the design analysis of list wrappers
(`igraph_vector_int_list_t`, `igraph_graph_list_t`, ...). Nothing here is part of
the library yet. Requires igraph 1.x.

- `inplace/` is the recommended design. A wrapper has exactly one data member, the
  igraph struct it owns, so a list wrapper hands out genuine `Vec &` references into
  the list's contiguous storage. Iterators are plain pointers; all standard
  algorithms and C++20 ranges work unchanged. C++17.
- `proxy/` is the alternative that was tried before: iterators dereference to a
  proxy "reference class". It shows exactly what C++20's `std::indirectly_writable`
  needs from such a proxy (a `const &&` assignment operator) and why the design
  still cannot be made safe (`auto x = list[i]` aliases). C++20.
- `main_issues.cpp` demonstrates the behavioural problems of the current wrappers on
  `main` (silent no-op `std::sort`, copies that are aliases, dropped move
  assignments, and more).

Build and run with sanitizers:

    cmake -S prototype -B build-proto -DCMAKE_PREFIX_PATH=<igraph 1.x install prefix>
    cmake --build build-proto
    ctest --test-dir build-proto --output-on-failure
