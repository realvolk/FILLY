#pragma once
#include "render.h"
#include "event.h"

typedef struct {
    bool (*setup)(void *self);
    bool (*draw)(void *self, RenderTree *tree);
    Event (*next_event)(void *self);
    bool (*teardown)(void *self);
    void (*get_size)(void *self, int *w, int *h);
} BackendVTable;

typedef struct {
    BackendVTable *vtable;
    void *data;
} Backend;