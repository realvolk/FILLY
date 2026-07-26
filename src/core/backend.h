#pragma once
#include "render.h"
#include "event.h"

typedef struct {
    bool (*setup)(void *self);
    bool (*draw)(void *self, RenderTree *tree);
    Event (*next_event)(void *self);
    bool (*teardown)(void *self);
    void (*get_size)(void *self, int *w, int *h);
    void (*wait_frame)(void *self);
    void (*copy_to_clipboard)(void *self, const char *text);
    char *(*paste_from_clipboard)(void *self);
    bool is_interactive;
} BackendVTable;

typedef struct {
    BackendVTable *vtable;
    void *data;
} Backend;