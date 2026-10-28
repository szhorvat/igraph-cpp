// Minimal stand-in for the parts of igraph used by list_a.hpp, mirroring igraph 1.0's
// struct layouts and semantics. Used only to compile/run the prototype on MSVC via Compiler Explorer.
#pragma once
#include <cstdint>
#include <cstdlib>
#include <cstring>
typedef int64_t igraph_int_t;
typedef bool igraph_bool_t;
typedef enum { IGRAPH_SUCCESS = 0, IGRAPH_ENOMEM = 2 } igraph_error_t;
inline const char *igraph_strerror(igraph_error_t) { return "error"; }
typedef struct { igraph_int_t *stor_begin, *stor_end, *end; } igraph_vector_int_t;
#define VECTOR(v) ((v).stor_begin)
inline igraph_error_t igraph_vector_int_init(igraph_vector_int_t *v, igraph_int_t n) {
    igraph_int_t a = n > 0 ? n : 1; v->stor_begin = (igraph_int_t *) calloc(a, sizeof(igraph_int_t));
    if (!v->stor_begin) return IGRAPH_ENOMEM; v->stor_end = v->stor_begin + a; v->end = v->stor_begin + n; return IGRAPH_SUCCESS; }
inline void igraph_vector_int_destroy(igraph_vector_int_t *v) { free(v->stor_begin); v->stor_begin = nullptr; }
inline igraph_int_t igraph_vector_int_size(const igraph_vector_int_t *v) { return v->end - v->stor_begin; }
inline igraph_error_t igraph_vector_int_resize(igraph_vector_int_t *v, igraph_int_t n) {
    if (n > v->stor_end - v->stor_begin) { igraph_int_t s = v->end - v->stor_begin; igraph_int_t *p = (igraph_int_t *) realloc(v->stor_begin, n * sizeof(igraph_int_t));
        if (!p) return IGRAPH_ENOMEM; v->stor_begin = p; v->stor_end = p + n; v->end = p + s; }
    v->end = v->stor_begin + n; return IGRAPH_SUCCESS; }
inline igraph_error_t igraph_vector_int_init_array(igraph_vector_int_t *v, const igraph_int_t *d, igraph_int_t n) {
    igraph_error_t e = igraph_vector_int_init(v, n); if (e) return e; if (n) memcpy(v->stor_begin, d, n * sizeof(igraph_int_t)); return e; }
inline igraph_error_t igraph_vector_int_init_copy(igraph_vector_int_t *v, const igraph_vector_int_t *f) { return igraph_vector_int_init_array(v, f->stor_begin, igraph_vector_int_size(f)); }
inline igraph_error_t igraph_vector_int_init_range(igraph_vector_int_t *v, igraph_int_t a, igraph_int_t b) {
    igraph_error_t e = igraph_vector_int_init(v, b - a); if (e) return e; for (igraph_int_t i = a; i < b; i++) v->stor_begin[i - a] = i; return e; }
inline igraph_error_t igraph_vector_int_update(igraph_vector_int_t *t, const igraph_vector_int_t *f) {
    igraph_int_t n = igraph_vector_int_size(f); igraph_error_t e = igraph_vector_int_resize(t, n); if (e) return e; if (n) memmove(t->stor_begin, f->stor_begin, n * sizeof(igraph_int_t)); return e; }
inline igraph_error_t igraph_vector_int_push_back(igraph_vector_int_t *v, igraph_int_t x) {
    igraph_int_t s = igraph_vector_int_size(v); if (v->end == v->stor_end) { igraph_int_t c = 2 * (v->stor_end - v->stor_begin);
        igraph_int_t *p = (igraph_int_t *) realloc(v->stor_begin, c * sizeof(igraph_int_t)); if (!p) return IGRAPH_ENOMEM; v->stor_begin = p; v->stor_end = p + c; v->end = p + s; }
    *v->end++ = x; return IGRAPH_SUCCESS; }
typedef struct { igraph_vector_int_t *stor_begin, *stor_end, *end; } igraph_vector_int_list_t;
inline igraph_error_t igraph_vector_int_list_reserve(igraph_vector_int_list_t *l, igraph_int_t c) {
    if (c <= l->stor_end - l->stor_begin) return IGRAPH_SUCCESS; igraph_int_t s = l->end - l->stor_begin;
    igraph_vector_int_t *p = (igraph_vector_int_t *) realloc(l->stor_begin, c * sizeof(igraph_vector_int_t)); if (!p) return IGRAPH_ENOMEM;
    l->stor_begin = p; l->stor_end = p + c; l->end = p + s; return IGRAPH_SUCCESS; }
inline igraph_error_t igraph_vector_int_list_resize(igraph_vector_int_list_t *l, igraph_int_t n) {
    igraph_int_t s = l->end - l->stor_begin; if (n > s) { igraph_error_t e = igraph_vector_int_list_reserve(l, n); if (e) return e;
        for (igraph_int_t i = s; i < n; i++) { e = igraph_vector_int_init(&l->stor_begin[i], 0); if (e) return e; l->end++; } }
    else { while (l->end - l->stor_begin > n) igraph_vector_int_destroy(--l->end); } return IGRAPH_SUCCESS; }
inline igraph_error_t igraph_vector_int_list_init(igraph_vector_int_list_t *l, igraph_int_t n) {
    l->stor_begin = (igraph_vector_int_t *) calloc(n > 0 ? n : 1, sizeof(igraph_vector_int_t)); if (!l->stor_begin) return IGRAPH_ENOMEM;
    l->stor_end = l->stor_begin + (n > 0 ? n : 1); l->end = l->stor_begin; return igraph_vector_int_list_resize(l, n); }
inline void igraph_vector_int_list_destroy(igraph_vector_int_list_t *l) { igraph_vector_int_list_resize(l, 0); free(l->stor_begin); }
inline void igraph_vector_int_list_clear(igraph_vector_int_list_t *l) { igraph_vector_int_list_resize(l, 0); }
inline igraph_error_t igraph_vector_int_list_push_back_new(igraph_vector_int_list_t *l, igraph_vector_int_t **r) {
    igraph_int_t s = l->end - l->stor_begin; if (l->end == l->stor_end) { igraph_error_t e = igraph_vector_int_list_reserve(l, 2 * s + 1); if (e) return e; }
    igraph_error_t e = igraph_vector_int_init(l->end, 0); if (e) return e; *r = l->end++; return IGRAPH_SUCCESS; }
inline igraph_error_t igraph_vector_int_list_push_back_copy(igraph_vector_int_list_t *l, const igraph_vector_int_t *v) {
    igraph_vector_int_t c; igraph_error_t e = igraph_vector_int_init_copy(&c, v); if (e) return e; igraph_vector_int_t *p;
    e = igraph_vector_int_list_push_back_new(l, &p); if (e) { igraph_vector_int_destroy(&c); return e; } igraph_vector_int_destroy(p); *p = c; return e; }
inline igraph_error_t igraph_vector_int_list_init_copy(igraph_vector_int_list_t *l, const igraph_vector_int_list_t *f) {
    igraph_error_t e = igraph_vector_int_list_init(l, 0); if (e) return e; for (auto *p = f->stor_begin; p != f->end; ++p) { e = igraph_vector_int_list_push_back_copy(l, p); if (e) return e; } return e; }
inline igraph_vector_int_t igraph_vector_int_list_pop_back(igraph_vector_int_list_t *l) { return *--l->end; }
inline void igraph_vector_int_list_discard(igraph_vector_int_list_t *l, igraph_int_t i) {
    igraph_vector_int_destroy(&l->stor_begin[i]); memmove(&l->stor_begin[i], &l->stor_begin[i + 1], (l->end - l->stor_begin - i - 1) * sizeof(igraph_vector_int_t)); l->end--; }
