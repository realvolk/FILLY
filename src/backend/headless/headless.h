#pragma once
#include "core/backend.h"
#include "core/render.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    char *buffer;
    int buf_size;
    uint32_t *pixels;
    int pix_width, pix_height;
    int term_w, term_h;
    Event *event_queue;
    int queue_head, queue_tail, queue_capacity;
    bool events_injected;
    bool auto_eof_sent;
} HeadlessBackend;

bool headless_backend_init(HeadlessBackend *hl, int w, int height);
bool headless_backend_init_pixel(HeadlessBackend *hl, int w, int height);
void headless_backend_destroy(HeadlessBackend *hl);
void headless_inject_key(HeadlessBackend *hl, KeyCode code, char ch);
void headless_inject_mouse(HeadlessBackend *hl, EventType type, int x, int y, int button);
void headless_inject_resize(HeadlessBackend *hl, int w, int height);
const char *headless_get_buffer(HeadlessBackend *hl);
const uint32_t *headless_get_pixels(HeadlessBackend *hl);
extern BackendVTable headless_vtable;
extern BackendVTable headless_pixel_vtable;