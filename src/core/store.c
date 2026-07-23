#include "store.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

typedef struct Subscriber_s {
    int client_fd;
    struct Subscriber_s *next;
} Subscriber;

typedef struct Entry_s {
    char *key;
    char *value;
    Subscriber *subs;
    struct Entry_s *next;
} Entry;

struct Store_s {
    Entry *head;
    int version;
};

Store *store_new(void) {
    Store *s = calloc(1, sizeof(Store));
    s->version = 0;
    return s;
}

void store_free(Store *s) {
    if (!s) return;
    Entry *e = s->head;
    while (e) {
        Entry *next = e->next;
        free(e->key);
        free(e->value);
        Subscriber *sub = e->subs;
        while (sub) {
            Subscriber *snext = sub->next;
            free(sub);
            sub = snext;
        }
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
            s->version++;
            Subscriber *sub = e->subs;
            while (sub) {
                char buf[512];
                snprintf(buf, sizeof(buf),
                    "{\"type\":\"state\",\"key\":\"%s\",\"value\":\"%s\"}\n",
                    key, value);
                write(sub->client_fd, buf, strlen(buf));
                sub = sub->next;
            }
            return;
        }
    }
    Entry *e = malloc(sizeof(Entry));
    e->key = strdup(key);
    e->value = strdup(value);
    e->subs = NULL;
    e->next = s->head;
    s->head = e;
    s->version++;
}

void store_subscribe(Store *s, const char *key, int client_fd) {
    for (Entry *e = s->head; e; e = e->next) {
        if (strcmp(e->key, key) == 0) {
            Subscriber *sub = malloc(sizeof(Subscriber));
            sub->client_fd = client_fd;
            sub->next = e->subs;
            e->subs = sub;
            return;
        }
    }
    Entry *e = malloc(sizeof(Entry));
    e->key = strdup(key);
    e->value = strdup("");
    Subscriber *sub = malloc(sizeof(Subscriber));
    sub->client_fd = client_fd;
    sub->next = NULL;
    e->subs = sub;
    e->next = s->head;
    s->head = e;
}

void store_unsubscribe(Store *s, const char *key, int client_fd) {
    for (Entry *e = s->head; e; e = e->next) {
        if (strcmp(e->key, key) == 0) {
            Subscriber **prev = &e->subs;
            while (*prev) {
                if ((*prev)->client_fd == client_fd) {
                    Subscriber *tmp = *prev;
                    *prev = tmp->next;
                    free(tmp);
                } else {
                    prev = &(*prev)->next;
                }
            }
            return;
        }
    }
}

bool store_get_bool(Store *s, const char *key) {
    const char *v = store_get(s, key);
    return v && strcmp(v, "yes") == 0;
}

bool store_enum(Store *s, int *idx, const char **key, const char **value) {
    if (!s) return false;
    int i = 0;
    Entry *e = s->head;
    while (e) {
        if (i == *idx) {
            *key = e->key;
            *value = e->value;
            (*idx)++;
            return true;
        }
        i++;
        e = e->next;
    }
    return false;
}

void store_get_version(Store *s, int *ver) {
    if (s) *ver = s->version;
    else *ver = 0;
}