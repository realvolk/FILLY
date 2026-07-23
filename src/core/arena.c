#include "arena.h"
#include <stdlib.h>
#include <string.h>

Arena *arena_new(size_t initial_capacity) {
    Arena *a = malloc(sizeof(Arena));
    if (!a) return NULL;
    a->buffer = malloc(initial_capacity);
    if (!a->buffer) {
        free(a);
        return NULL;
    }
    a->capacity = initial_capacity;
    a->offset = 0;
    return a;
}

void arena_reset(Arena *a) {
    if (!a) return;
    a->offset = 0;
}

void *arena_alloc(Arena *a, size_t size) {
    if (!a) return NULL;
    size_t aligned = (size + 7) & ~7;
    if (a->offset + aligned > a->capacity) {
        size_t new_cap = a->capacity * 2;
        while (new_cap < a->offset + aligned) new_cap *= 2;
        char *new_buf = realloc(a->buffer, new_cap);
        if (!new_buf) return NULL;
        a->buffer = new_buf;
        a->capacity = new_cap;
    }
    void *ptr = a->buffer + a->offset;
    a->offset += aligned;
    return ptr;
}

char *arena_strdup(Arena *a, const char *src) {
    if (!a || !src) return NULL;
    size_t len = strlen(src) + 1;
    char *dst = arena_alloc(a, len);
    if (!dst) return NULL;
    memcpy(dst, src, len);
    return dst;
}

void arena_maybe_shrink(Arena *a) {
    if (!a) return;
    size_t used = a->offset;
    size_t cap = a->capacity;
    if (cap > 4 * 1024 * 1024 && used < cap / 4) {
        size_t new_cap = used * 2;
        if (new_cap < 1024 * 1024) new_cap = 1024 * 1024;
        char *new_buf = realloc(a->buffer, new_cap);
        if (new_buf) {
            a->buffer = new_buf;
            a->capacity = new_cap;
        }
    }
}

void arena_free(Arena *a) {
    if (!a) return;
    free(a->buffer);
    free(a);
}