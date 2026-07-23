#pragma once
#include <stddef.h>

typedef struct {
    char *buffer;
    size_t capacity;
    size_t offset;
} Arena;

Arena *arena_new(size_t initial_capacity);
void arena_reset(Arena *a);
void *arena_alloc(Arena *a, size_t size);
char *arena_strdup(Arena *a, const char *src);
void arena_free(Arena *a);
void arena_maybe_shrink(Arena *a);