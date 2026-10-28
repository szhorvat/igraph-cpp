# Experiment: list wrappers with real references

Question: can wrappers of igraph's typed lists (`igraph_vector_int_list_t`, ...) work with
standard library algorithms without proxy references, and without behaviour that surprises users?

Approach: the element wrapper (`IntVec`) is a standard-layout class whose only data member is the
igraph struct (`igraph_vector_int_t`), so it is layout-identical to it. The list's storage, an array
of `igraph_vector_int_t`, is then viewed as an array of `IntVec`. List iterators are `IntVec *`,
references are `IntVec &`. No proxies are involved.

Invariant: every `IntVec`, including a moved-from one, is a valid igraph vector. Move construction
steals the storage and leaves the source as a valid empty vector; move assignment swaps.
This keeps the list valid for igraph at all times, e.g. after `std::remove_if` without `erase`.

Consequences:
- `auto x = list[i]` is a copy, `auto &x = list[i]` is a reference, exactly as with `std::vector`.
- `push_back(v)` copies, `push_back(std::move(v))` moves.
- Legacy (C++17) and C++20 range algorithms work; `IntVecList` models `contiguous_range` and its
  iterator models `sortable`.
- There is no "alias mode" in the wrapper. A non-owning view of an existing `igraph_vector_int_t`
  is a plain reference: `IntVec &v = IntVec::view(raw)`.

Caveat: viewing igraph-owned storage as `IntVec` objects is formally undefined behaviour under the
C++ object model, since no `IntVec` object was ever constructed there. It relies on layout
compatibility (checked with `static_assert`) and on compilers not exploiting this, which GCC,
Clang and MSVC do not.

`mock_igraph.h` is a minimal stand-in for igraph, allowing the test to be compiled without igraph
(e.g. on Compiler Explorer): concatenate `mock_igraph.h`, `contiguous_list.hpp` and the test, and
drop the sections of the test that call real igraph functions.
