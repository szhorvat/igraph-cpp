// Prototype: "in-place" wrappers for igraph containers and lists of containers.
//
// Design in one sentence: a wrapper class has exactly one data member, the igraph
// struct it owns, so that a pointer to the igraph struct can be reinterpreted as a
// pointer to the wrapper. Lists of igraph structs (igraph_vector_int_list_t,
// igraph_graph_list_t, ...) store their items contiguously by value, so indexing a
// list wrapper can return a genuine C++ reference (Vec &) into the list's storage.
// There are no proxy references and no iterator classes: the iterator is Vec *.
//
// Consequences:
//  - All standard algorithms and C++20 ranges work unchanged, because the iterator
//    is a plain pointer (a contiguous iterator) and the reference type is a real
//    reference. 'auto x = list[i]' makes a deep copy, 'auto &x = list[i]' makes a
//    reference, exactly as for std::vector<T>.
//  - The owning/aliasing dual mode of the old wrappers is gone. A wrapper always owns
//    its struct. To use the wrapper interface on an externally owned igraph struct,
//    reinterpret a reference to it (Vec::alias(v)); that creates no new object and
//    nothing is destroyed when the reference goes away.
//  - Moving a wrapper swaps structs; a moved-from wrapper is a valid empty container.
//
// This prototype covers only Vec<igraph_int_t>, Vec<igraph_real_t>, VecList<...>,
// Graph and GraphList, enough to test the design. It requires C++17 and igraph 1.x.

#ifndef IGCPP_INPLACE_HPP
#define IGCPP_INPLACE_HPP

#include <igraph.h>

#if IGRAPH_VERSION_MAJOR < 1
#error "This prototype requires igraph 1.x."
#endif

#include <cstddef>
#include <initializer_list>
#include <iterator>
#include <new>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace ig {

struct Exception : std::runtime_error {
    igraph_error_t error;
    explicit Exception(igraph_error_t error_) :
        std::runtime_error(igraph_strerror(error_)), error(error_) { }
};

inline void check(igraph_error_t error) {
    if (error != IGRAPH_SUCCESS) {
        throw Exception{error};
    }
}

// Tag type for the constructors that take over ownership of an igraph struct.
// The struct is copied by value (it is a small handle); the original must not be
// used or destroyed afterwards.
template<typename T>
struct CaptureType {
    T obj;
};

template<typename T>
CaptureType<T> Capture(const T &obj) { return CaptureType<T>{obj}; }

// ---------------------------------------------------------------------------
// Traits mapping an element type to the igraph vector type and its functions.
// ---------------------------------------------------------------------------

template<typename Base> struct vec_traits;

#define IGCPP_VEC_TRAITS(BASE, PREFIX, LIST_PREFIX)                                           \
    template<> struct vec_traits<BASE> {                                                   \
        using igraph_type = PREFIX##_t;                                                    \
        using list_type = LIST_PREFIX##_t;                                                 \
        static igraph_error_t init(igraph_type *v, igraph_int_t n) { return PREFIX##_init(v, n); } \
        static igraph_error_t init_copy(igraph_type *to, const igraph_type *from) { return PREFIX##_init_copy(to, from); } \
        static igraph_error_t init_array(igraph_type *v, const BASE *data, igraph_int_t n) { return PREFIX##_init_array(v, data, n); } \
        static void destroy(igraph_type *v) { PREFIX##_destroy(v); }                       \
        static igraph_error_t update(igraph_type *to, const igraph_type *from) { return PREFIX##_update(to, from); } \
        static void swap(igraph_type *a, igraph_type *b) { PREFIX##_swap(a, b); }            \
        static igraph_error_t resize(igraph_type *v, igraph_int_t n) { return PREFIX##_resize(v, n); } \
        static igraph_error_t reserve(igraph_type *v, igraph_int_t n) { return PREFIX##_reserve(v, n); } \
        static void clear(igraph_type *v) { PREFIX##_clear(v); }                           \
        static igraph_error_t push_back(igraph_type *v, BASE e) { return PREFIX##_push_back(v, e); } \
        static BASE pop_back(igraph_type *v) { return PREFIX##_pop_back(v); }              \
        static void remove_section(igraph_type *v, igraph_int_t from, igraph_int_t to) { PREFIX##_remove_section(v, from, to); } \
        static igraph_bool_t all_e(const igraph_type *a, const igraph_type *b) { return PREFIX##_all_e(a, b); } \
        /* list functions */                                                               \
        static igraph_error_t list_init(list_type *l, igraph_int_t n) { return LIST_PREFIX##_init(l, n); } \
        static igraph_error_t list_init_copy(list_type *to, const list_type *from) { return LIST_PREFIX##_init_copy(to, from); } \
        static void list_destroy(list_type *l) { LIST_PREFIX##_destroy(l); }               \
        static void list_swap(list_type *a, list_type *b) { LIST_PREFIX##_swap(a, b); }      \
        static igraph_error_t list_resize(list_type *l, igraph_int_t n) { return LIST_PREFIX##_resize(l, n); } \
        static igraph_error_t list_reserve(list_type *l, igraph_int_t n) { return LIST_PREFIX##_reserve(l, n); } \
        static void list_clear(list_type *l) { LIST_PREFIX##_clear(l); }                   \
        static igraph_error_t list_push_back_copy(list_type *l, const igraph_type *e) { return LIST_PREFIX##_push_back_copy(l, e); } \
        static igraph_error_t list_push_back_new(list_type *l, igraph_type **e) { return LIST_PREFIX##_push_back_new(l, e); } \
        static igraph_error_t list_insert_copy(list_type *l, igraph_int_t pos, const igraph_type *e) { return LIST_PREFIX##_insert_copy(l, pos, e); } \
        static igraph_error_t list_insert_new(list_type *l, igraph_int_t pos, igraph_type **e) { return LIST_PREFIX##_insert_new(l, pos, e); } \
        static igraph_type list_pop_back(list_type *l) { return LIST_PREFIX##_pop_back(l); } \
        static void list_discard(list_type *l, igraph_int_t pos) { LIST_PREFIX##_discard(l, pos); } \
        static igraph_error_t list_remove(list_type *l, igraph_int_t pos, igraph_type *e) { return LIST_PREFIX##_remove(l, pos, e); } \
    };

IGCPP_VEC_TRAITS(igraph_int_t, igraph_vector_int, igraph_vector_int_list)
IGCPP_VEC_TRAITS(igraph_real_t, igraph_vector, igraph_vector_list)

#undef IGCPP_VEC_TRAITS

// ---------------------------------------------------------------------------
// Vec: owning wrapper of an igraph vector, layout-compatible with it.
// ---------------------------------------------------------------------------

template<typename Base>
class Vec {
    using traits = vec_traits<Base>;

public:
    using igraph_type = typename traits::igraph_type;

    using value_type = Base;
    using reference = value_type &;
    using const_reference = const value_type &;
    using pointer = value_type *;
    using const_pointer = const value_type *;
    using iterator = value_type *;
    using const_iterator = const value_type *;
    using difference_type = igraph_int_t;
    using size_type = igraph_int_t;

private:
    igraph_type vec;   // the one and only data member

public:
    // --- construction / destruction ---

    Vec() { check(traits::init(&vec, 0)); }

    explicit Vec(size_type n) { check(traits::init(&vec, n)); }

    Vec(std::initializer_list<value_type> list) {
        check(traits::init_array(&vec, list.begin(), static_cast<igraph_int_t>(list.size())));
    }

    // Takes over ownership of 'v'; 'v' must not be used or destroyed afterwards.
    explicit Vec(CaptureType<igraph_type> v) : vec(v.obj) { }

    // Deep copy from an igraph vector.
    explicit Vec(const igraph_type *v) { check(traits::init_copy(&vec, v)); }

    Vec(const Vec &other) { check(traits::init_copy(&vec, &other.vec)); }

    // The moved-from vector is left as a valid empty vector. If allocating that empty
    // vector fails, the moved-from vector is left in igraph's "null" state, which is
    // safe to destroy but not to use otherwise.
    Vec(Vec &&other) noexcept {
        vec = other.vec;
        if (traits::init(&other.vec, 0) != IGRAPH_SUCCESS) {
            other.vec.stor_begin = nullptr;
        }
    }

    Vec & operator = (const Vec &other) {
        check(traits::update(&vec, &other.vec));
        return *this;
    }

    // Swaps contents; the moved-from vector ends up holding the previous contents of
    // *this, which is fine since a moved-from object is only required to be valid.
    Vec & operator = (Vec &&other) noexcept {
        traits::swap(&vec, &other.vec);
        return *this;
    }

    ~Vec() { traits::destroy(&vec); }

    // --- aliasing: wrapper interface for an externally owned igraph vector ---

    // Returns a reference to 'v' seen through the wrapper interface. No object is
    // created and nothing is destroyed when the reference is no longer used; the
    // referenced igraph vector must outlive all uses of the reference.
    static Vec & alias(igraph_type &v) noexcept { return *reinterpret_cast<Vec *>(&v); }
    static const Vec & alias(const igraph_type &v) noexcept { return *reinterpret_cast<const Vec *>(&v); }

    // --- interop with igraph ---

    igraph_type * get() noexcept { return &vec; }
    const igraph_type * get() const noexcept { return &vec; }

    operator igraph_type * () noexcept { return &vec; }
    operator const igraph_type * () const noexcept { return &vec; }

    // --- container interface ---

    iterator begin() noexcept { return vec.stor_begin; }
    iterator end() noexcept { return vec.end; }
    const_iterator begin() const noexcept { return vec.stor_begin; }
    const_iterator end() const noexcept { return vec.end; }
    const_iterator cbegin() const noexcept { return begin(); }
    const_iterator cend() const noexcept { return end(); }

    pointer data() noexcept { return vec.stor_begin; }
    const_pointer data() const noexcept { return vec.stor_begin; }

    size_type size() const noexcept { return vec.end - vec.stor_begin; }
    size_type capacity() const noexcept { return vec.stor_end - vec.stor_begin; }
    bool empty() const noexcept { return vec.end == vec.stor_begin; }

    reference operator [] (size_type i) noexcept { return vec.stor_begin[i]; }
    const_reference operator [] (size_type i) const noexcept { return vec.stor_begin[i]; }

    reference front() noexcept { return *begin(); }
    const_reference front() const noexcept { return *begin(); }
    reference back() noexcept { return *(end() - 1); }
    const_reference back() const noexcept { return *(end() - 1); }

    void clear() noexcept { traits::clear(&vec); }
    void resize(size_type n) { check(traits::resize(&vec, n)); }
    void reserve(size_type n) { check(traits::reserve(&vec, n)); }

    void push_back(value_type e) { check(traits::push_back(&vec, e)); }
    value_type pop_back() noexcept { return traits::pop_back(&vec); }

    iterator erase(const_iterator first, const_iterator last) noexcept {
        traits::remove_section(&vec, first - cbegin(), last - cbegin());
        return begin() + (first - cbegin());
    }
    iterator erase(const_iterator pos) noexcept { return erase(pos, pos + 1); }

    friend void swap(Vec &a, Vec &b) noexcept { traits::swap(&a.vec, &b.vec); }

    friend bool operator == (const Vec &a, const Vec &b) noexcept { return traits::all_e(&a.vec, &b.vec); }
    friend bool operator != (const Vec &a, const Vec &b) noexcept { return !(a == b); }
};

using IntVec = Vec<igraph_int_t>;
using RealVec = Vec<igraph_real_t>;

// The layout requirements that the alias() functions and the list wrappers rely on.
static_assert(std::is_standard_layout<IntVec>::value, "Vec must be standard-layout");
static_assert(sizeof(IntVec) == sizeof(igraph_vector_int_t), "Vec must have no extra members");
static_assert(std::is_standard_layout<RealVec>::value, "Vec must be standard-layout");
static_assert(sizeof(RealVec) == sizeof(igraph_vector_t), "Vec must have no extra members");

// ---------------------------------------------------------------------------
// VecList: owning wrapper of an igraph vector list, with Vec & element access.
// ---------------------------------------------------------------------------

template<typename Base>
class VecList {
    using traits = vec_traits<Base>;

public:
    using igraph_type = typename traits::list_type;
    using item_type = typename traits::igraph_type;

    using value_type = Vec<Base>;
    using reference = value_type &;
    using const_reference = const value_type &;
    using pointer = value_type *;
    using const_pointer = const value_type *;
    using iterator = value_type *;
    using const_iterator = const value_type *;
    using difference_type = igraph_int_t;
    using size_type = igraph_int_t;

private:
    igraph_type list;   // the one and only data member

    static pointer wrap(item_type *p) noexcept { return reinterpret_cast<pointer>(p); }
    static const_pointer wrap(const item_type *p) noexcept { return reinterpret_cast<const_pointer>(p); }

public:
    VecList() { check(traits::list_init(&list, 0)); }
    explicit VecList(size_type n) { check(traits::list_init(&list, n)); }

    explicit VecList(CaptureType<igraph_type> l) : list(l.obj) { }
    explicit VecList(const igraph_type *l) { check(traits::list_init_copy(&list, l)); }

    VecList(const VecList &other) { check(traits::list_init_copy(&list, &other.list)); }

    VecList(VecList &&other) noexcept {
        list = other.list;
        if (traits::list_init(&other.list, 0) != IGRAPH_SUCCESS) {
            other.list.stor_begin = nullptr;
        }
    }

    VecList & operator = (const VecList &other) {
        VecList tmp(other);
        traits::list_swap(&list, &tmp.list);
        return *this;
    }

    VecList & operator = (VecList &&other) noexcept {
        traits::list_swap(&list, &other.list);
        return *this;
    }

    ~VecList() { traits::list_destroy(&list); }

    static VecList & alias(igraph_type &l) noexcept { return *reinterpret_cast<VecList *>(&l); }
    static const VecList & alias(const igraph_type &l) noexcept { return *reinterpret_cast<const VecList *>(&l); }

    igraph_type * get() noexcept { return &list; }
    const igraph_type * get() const noexcept { return &list; }

    operator igraph_type * () noexcept { return &list; }
    operator const igraph_type * () const noexcept { return &list; }

    // --- element access: genuine references into the list's storage ---

    iterator begin() noexcept { return wrap(list.stor_begin); }
    iterator end() noexcept { return wrap(list.end); }
    const_iterator begin() const noexcept { return wrap(list.stor_begin); }
    const_iterator end() const noexcept { return wrap(list.end); }
    const_iterator cbegin() const noexcept { return begin(); }
    const_iterator cend() const noexcept { return end(); }

    pointer data() noexcept { return begin(); }
    const_pointer data() const noexcept { return begin(); }

    size_type size() const noexcept { return list.end - list.stor_begin; }
    size_type capacity() const noexcept { return list.stor_end - list.stor_begin; }
    bool empty() const noexcept { return list.end == list.stor_begin; }

    reference operator [] (size_type i) noexcept { return begin()[i]; }
    const_reference operator [] (size_type i) const noexcept { return begin()[i]; }

    reference front() noexcept { return *begin(); }
    const_reference front() const noexcept { return *begin(); }
    reference back() noexcept { return *(end() - 1); }
    const_reference back() const noexcept { return *(end() - 1); }

    // --- modifiers; invalidation rules are those of std::vector ---

    void clear() noexcept { traits::list_clear(&list); }
    void resize(size_type n) { check(traits::list_resize(&list, n)); }
    void reserve(size_type n) { check(traits::list_reserve(&list, n)); }

    // Appends a deep copy.
    void push_back(const value_type &v) { check(traits::list_push_back_copy(&list, v.get())); }

    // Appends by moving; 'v' is left as a valid empty vector.
    void push_back(value_type &&v) {
        item_type *slot;
        check(traits::list_push_back_new(&list, &slot));
        traits::swap(slot, v.get());
    }

    // Appends a new empty vector and returns a reference to it.
    reference emplace_back() {
        item_type *slot;
        check(traits::list_push_back_new(&list, &slot));
        return *wrap(slot);
    }

    // Removes the last element and returns it (ownership moves to the caller).
    value_type pop_back() noexcept {
        return value_type(Capture(traits::list_pop_back(&list)));
    }

    iterator insert(const_iterator pos, const value_type &v) {
        const size_type i = pos - cbegin();
        check(traits::list_insert_copy(&list, i, v.get()));
        return begin() + i;
    }

    iterator insert(const_iterator pos, value_type &&v) {
        const size_type i = pos - cbegin();
        item_type *slot;
        check(traits::list_insert_new(&list, i, &slot));
        traits::swap(slot, v.get());
        return begin() + i;
    }

    // Destroys the element at 'pos'. Like std::vector::erase, this shifts the tail.
    iterator erase(const_iterator pos) noexcept {
        const size_type i = pos - cbegin();
        traits::list_discard(&list, i);
        return begin() + i;
    }

    iterator erase(const_iterator first, const_iterator last) noexcept {
        const size_type i = first - cbegin();
        for (size_type k = last - first; k > 0; --k) {
            traits::list_discard(&list, i);
        }
        return begin() + i;
    }

    // Removes the element at 'pos' and returns it to the caller instead of destroying it.
    value_type take(const_iterator pos) {
        item_type item;
        check(traits::list_remove(&list, pos - cbegin(), &item));
        return value_type(Capture(item));
    }

    friend void swap(VecList &a, VecList &b) noexcept { traits::list_swap(&a.list, &b.list); }

    friend bool operator == (const VecList &a, const VecList &b) noexcept {
        if (a.size() != b.size()) {
            return false;
        }
        for (size_type i = 0; i < a.size(); ++i) {
            if (!(a[i] == b[i])) {
                return false;
            }
        }
        return true;
    }
    friend bool operator != (const VecList &a, const VecList &b) noexcept { return !(a == b); }
};

using IntVecList = VecList<igraph_int_t>;
using RealVecList = VecList<igraph_real_t>;

static_assert(std::is_standard_layout<IntVecList>::value, "VecList must be standard-layout");
static_assert(sizeof(IntVecList) == sizeof(igraph_vector_int_list_t), "VecList must have no extra members");

// ---------------------------------------------------------------------------
// Graph and GraphList, the same way.
// ---------------------------------------------------------------------------

class Graph {
    igraph_t graph;   // the one and only data member

    // igraph_destroy() must not be called on a graph whose construction failed.
    // We mark such a graph by a null edge vector, which never occurs in a live graph.
    bool is_dead() const noexcept { return graph.from.stor_begin == nullptr; }
    void mark_dead() noexcept { graph.from.stor_begin = nullptr; }

public:
    using igraph_type = igraph_t;

    explicit Graph(igraph_int_t vcount = 0, bool directed = false) {
        check(igraph_empty(&graph, vcount, directed));
    }

    Graph(const igraph_vector_int_t *edges, igraph_int_t vcount, bool directed) {
        check(igraph_create(&graph, edges, vcount, directed));
    }

    explicit Graph(CaptureType<igraph_t> g) : graph(g.obj) { }
    explicit Graph(const igraph_t *g) { check(igraph_copy(&graph, g)); }

    Graph(const Graph &other) { check(igraph_copy(&graph, &other.graph)); }

    Graph(Graph &&other) noexcept {
        graph = other.graph;
        if (igraph_empty(&other.graph, 0, IGRAPH_UNDIRECTED) != IGRAPH_SUCCESS) {
            other.mark_dead();
        }
    }

    Graph & operator = (const Graph &other) {
        Graph tmp(other);
        std::swap(graph, tmp.graph);
        return *this;
    }

    Graph & operator = (Graph &&other) noexcept {
        std::swap(graph, other.graph);
        return *this;
    }

    ~Graph() {
        if (!is_dead()) {
            igraph_destroy(&graph);
        }
    }

    static Graph & alias(igraph_t &g) noexcept { return *reinterpret_cast<Graph *>(&g); }
    static const Graph & alias(const igraph_t &g) noexcept { return *reinterpret_cast<const Graph *>(&g); }

    igraph_t * get() noexcept { return &graph; }
    const igraph_t * get() const noexcept { return &graph; }
    operator igraph_t * () noexcept { return &graph; }
    operator const igraph_t * () const noexcept { return &graph; }

    igraph_int_t vcount() const noexcept { return igraph_vcount(&graph); }
    igraph_int_t ecount() const noexcept { return igraph_ecount(&graph); }
    bool is_directed() const noexcept { return igraph_is_directed(&graph); }

    friend void swap(Graph &a, Graph &b) noexcept { std::swap(a.graph, b.graph); }

    // Compares labelled graphs (same vertex count, same edge list), ignoring attributes.
    friend bool operator == (const Graph &a, const Graph &b) {
        igraph_bool_t res;
        check(igraph_is_same_graph(&a.graph, &b.graph, &res));
        return res;
    }
    friend bool operator != (const Graph &a, const Graph &b) { return !(a == b); }
};

static_assert(std::is_standard_layout<Graph>::value, "Graph must be standard-layout");
static_assert(sizeof(Graph) == sizeof(igraph_t), "Graph must have no extra members");

class GraphList {
    igraph_graph_list_t list;

    static Graph * wrap(igraph_t *p) noexcept { return reinterpret_cast<Graph *>(p); }
    static const Graph * wrap(const igraph_t *p) noexcept { return reinterpret_cast<const Graph *>(p); }

public:
    using igraph_type = igraph_graph_list_t;
    using value_type = Graph;
    using reference = Graph &;
    using const_reference = const Graph &;
    using iterator = Graph *;
    using const_iterator = const Graph *;
    using difference_type = igraph_int_t;
    using size_type = igraph_int_t;

    GraphList() { check(igraph_graph_list_init(&list, 0)); }
    explicit GraphList(size_type n) { check(igraph_graph_list_init(&list, n)); }
    explicit GraphList(CaptureType<igraph_graph_list_t> l) : list(l.obj) { }

    GraphList(const GraphList &other) { check(igraph_graph_list_init_copy(&list, &other.list)); }
    GraphList(GraphList &&other) noexcept {
        list = other.list;
        if (igraph_graph_list_init(&other.list, 0) != IGRAPH_SUCCESS) {
            other.list.stor_begin = nullptr;
        }
    }
    GraphList & operator = (const GraphList &other) {
        GraphList tmp(other);
        igraph_graph_list_swap(&list, &tmp.list);
        return *this;
    }
    GraphList & operator = (GraphList &&other) noexcept {
        igraph_graph_list_swap(&list, &other.list);
        return *this;
    }
    ~GraphList() { igraph_graph_list_destroy(&list); }

    static GraphList & alias(igraph_graph_list_t &l) noexcept { return *reinterpret_cast<GraphList *>(&l); }

    igraph_graph_list_t * get() noexcept { return &list; }
    const igraph_graph_list_t * get() const noexcept { return &list; }
    operator igraph_graph_list_t * () noexcept { return &list; }
    operator const igraph_graph_list_t * () const noexcept { return &list; }

    iterator begin() noexcept { return wrap(list.stor_begin); }
    iterator end() noexcept { return wrap(list.end); }
    const_iterator begin() const noexcept { return wrap(list.stor_begin); }
    const_iterator end() const noexcept { return wrap(list.end); }
    const_iterator cbegin() const noexcept { return begin(); }
    const_iterator cend() const noexcept { return end(); }

    size_type size() const noexcept { return list.end - list.stor_begin; }
    bool empty() const noexcept { return list.end == list.stor_begin; }

    reference operator [] (size_type i) noexcept { return begin()[i]; }
    const_reference operator [] (size_type i) const noexcept { return begin()[i]; }
    reference back() noexcept { return *(end() - 1); }
    const_reference back() const noexcept { return *(end() - 1); }

    void clear() noexcept { igraph_graph_list_clear(&list); }
    void reserve(size_type n) { check(igraph_graph_list_reserve(&list, n)); }

    void push_back(const Graph &g) { check(igraph_graph_list_push_back_copy(&list, g.get())); }
    void push_back(Graph &&g) {
        igraph_t *slot;
        check(igraph_graph_list_push_back_new(&list, &slot));
        swap(*wrap(slot), g);
    }
    Graph pop_back() noexcept { return Graph(Capture(igraph_graph_list_pop_back(&list))); }

    iterator erase(const_iterator pos) noexcept {
        const size_type i = pos - cbegin();
        igraph_graph_list_discard(&list, i);
        return begin() + i;
    }

    friend void swap(GraphList &a, GraphList &b) noexcept { igraph_graph_list_swap(&a.list, &b.list); }
};

static_assert(std::is_standard_layout<GraphList>::value, "GraphList must be standard-layout");
static_assert(sizeof(GraphList) == sizeof(igraph_graph_list_t), "GraphList must have no extra members");

} // namespace ig

#endif // IGCPP_INPLACE_HPP
