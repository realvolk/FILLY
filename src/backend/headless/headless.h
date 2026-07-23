#pragma once
#include "core/backend.h"
#include "core/render.h"
#include <stdbool.h>

typedef struct {
    char *buffer;
    int buf_size;
    int term_w, term_h;
    Event *event_queue;
    int queue_head, queue_tail, queue_capacity;
    bool events_injected;
    bool auto_eof_sent;
} HeadlessBackend;

bool headless_backend_init(HeadlessBackend *hl, int w, int height);
void headless_backend_destroy(HeadlessBackend *hl);
void headless_inject_key(HeadlessBackend *hl, KeyCode code, char ch);
void headless_inject_resize(HeadlessBackend *hl, int w, int height);
const char *headless_get_buffer(HeadlessBackend *hl);
extern BackendVTable headless_vtable;