#include "store.h"
#include <stdlib.h>
#include <string.h>

typedef struct Entry_s {
    char *key;
    char *value;
    struct Entry_s *next;
} Entry;

struct Store_s {
    Entry *head;
};

Store *store_new(void) {
    return calloc(1, sizeof(Store));
}

void store_free(Store *s) {
    if (!s) return;
    Entry *e = s->head;
    while (e) {
        Entry *next = e->next;
        free(e->key);
        free(e->value);
        free(e);
        e = next;
    }
    free(s);
}

const char *store_get(Store *s, const char *key) {
    for (Entry *e = s->head; e; e = e->next)
        if (strcmp(e->key, key) == 0) return e->value;
    return NULL;
}

void store_set(Store *s, const char *key, const char *value) {
    for (Entry *e = s->head; e; e = e->next) {
        if (strcmp(e->key, key) == 0) {
            free(e->value);
            e->value = strdup(value);
            return;
        }
    }
    Entry *e = malloc(sizeof(Entry));
    e->key = strdup(key);
    e->value = strdup(value);
    e->next = s->head;
    s->head = e;
}

bool store_get_bool(Store *s, const char *key) {
    const char *v = store_get(s, key);
    return v && strcmp(v, "yes") == 0;
}