#include "core/arena.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

int main(void) {
    printf("=== Arena Tests ===\n");

    Arena *a = arena_new(1024);
    assert(a != NULL);
    assert(a->capacity >= 1024);
    assert(a->offset == 0);

    char *s1 = arena_alloc(a, 64);
    char *s2 = arena_alloc(a, 128);
    assert(s1 != NULL && s2 != NULL);
    assert(s2 - s1 >= 64);
    assert(a->offset >= 192);

    arena_reset(a);
    assert(a->offset == 0);

    char *s3 = arena_alloc(a, 64);
    assert(s3 == s1);

    char *dup = arena_strdup(a, "hello");
    assert(strcmp(dup, "hello") == 0);

    for (int i = 0; i < 100; i++) arena_alloc(a, 1024);
    assert(a->capacity >= 100 * 1024);

    arena_reset(a);
    for (int i = 0; i < 5000; i++) arena_alloc(a, 1024);
    assert(a->capacity >= 5000 * 1024);
    arena_reset(a);
    for (int i = 0; i < 10; i++) arena_alloc(a, 1024);
    size_t cap_before = a->capacity;
    arena_maybe_shrink(a);
    assert(a->capacity < cap_before);

    arena_free(a);
    printf("PASS: All arena tests\n");
    return 0;
}