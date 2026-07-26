#include "headless.h"
#include "backend/terminal/renderer.h"
#include "backend/gcore/renderer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool hl_setup(void *self) { (void)self; return true; }

static bool hl_draw(void *self, RenderTree *tree) {
    HeadlessBackend *hl = (HeadlessBackend *)self;
    if (!hl->buffer) {
        hl->buf_size = hl->term_w * hl->term_h * 32;
        hl->buffer = calloc(1, hl->buf_size);
    }
    render_tree_to_buf(tree, 0, 0, hl->term_w, hl->term_h, hl->buffer, hl->buf_size);
    return true;
}

static bool hl_pixel_draw(void *self, RenderTree *tree) {
    HeadlessBackend *hl = (HeadlessBackend *)self;
    if (!hl->pixels) {
        hl->pixels = calloc(hl->pix_width * hl->pix_height, sizeof(uint32_t));
    }
    PixelBuffer pb = { .pixels = hl->pixels, .width = hl->pix_width,
                       .height = hl->pix_height, .stride = hl->pix_width,
                       .mouse_x = 0, .mouse_y = 0 };
    gcore_render_tree_to_pixels(tree, &pb, NULL, NULL);
    return true;
}

static Event hl_next_event(void *self) {
    HeadlessBackend *hl = (HeadlessBackend *)self;
    if (hl->queue_head == hl->queue_tail) {
        if (hl->events_injected && !hl->auto_eof_sent) {
            hl->auto_eof_sent = true;
            Event ev = { .type = EVENT_KEY, .code = KEY_ESC, .ch = 0 };
            return ev;
        }
        Event ev = { .type = EVENT_NONE };
        return ev;
    }
    hl->events_injected = true;
    Event ev = hl->event_queue[hl->queue_head];
    hl->queue_head = (hl->queue_head + 1) % hl->queue_capacity;
    return ev;
}

static bool hl_teardown(void *self) { (void)self; return true; }

static void hl_get_size(void *self, int *w, int *h) {
    HeadlessBackend *hl = (HeadlessBackend *)self;
    *w = hl->term_w; *h = hl->term_h;
}

BackendVTable headless_vtable = {
    .setup = hl_setup, .draw = hl_draw, .next_event = hl_next_event,
    .teardown = hl_teardown, .get_size = hl_get_size,
    .wait_frame = NULL,
    .copy_to_clipboard = NULL,
    .paste_from_clipboard = NULL,
    .is_interactive = false,
};

BackendVTable headless_pixel_vtable = {
    .setup = hl_setup, .draw = hl_pixel_draw, .next_event = hl_next_event,
    .teardown = hl_teardown, .get_size = hl_get_size,
    .wait_frame = NULL,
    .copy_to_clipboard = NULL,
    .paste_from_clipboard = NULL,
    .is_interactive = false,
};

bool headless_backend_init(HeadlessBackend *hl, int w, int height) {
    memset(hl, 0, sizeof(*hl));
    hl->term_w = w; hl->term_h = height;
    hl->pix_width = 0; hl->pix_height = 0;
    hl->pixels = NULL;
    hl->queue_capacity = 128;
    hl->event_queue = calloc(hl->queue_capacity, sizeof(Event));
    hl->queue_head = hl->queue_tail = 0;
    hl->events_injected = false;
    hl->auto_eof_sent = false;
    return true;
}

bool headless_backend_init_pixel(HeadlessBackend *hl, int w, int height) {
    memset(hl, 0, sizeof(*hl));
    hl->pix_width = w; hl->pix_height = height;
    hl->term_w = w; hl->term_h = height;
    hl->pixels = NULL;
    hl->queue_capacity = 128;
    hl->event_queue = calloc(hl->queue_capacity, sizeof(Event));
    hl->queue_head = hl->queue_tail = 0;
    hl->events_injected = false;
    hl->auto_eof_sent = false;
    return true;
}

void headless_backend_destroy(HeadlessBackend *hl) {
    free(hl->buffer);
    free(hl->pixels);
    free(hl->event_queue);
    memset(hl, 0, sizeof(*hl));
}

void headless_inject_key(HeadlessBackend *hl, KeyCode code, char ch) {
    int next = (hl->queue_tail + 1) % hl->queue_capacity;
    if (next == hl->queue_head) return;
    hl->event_queue[hl->queue_tail].type = EVENT_KEY;
    hl->event_queue[hl->queue_tail].code = code;
    hl->event_queue[hl->queue_tail].ch = ch;
    hl->queue_tail = next;
}

void headless_inject_mouse(HeadlessBackend *hl, EventType type, int x, int y, int button) {
    int next = (hl->queue_tail + 1) % hl->queue_capacity;
    if (next == hl->queue_head) return;
    hl->event_queue[hl->queue_tail].type = type;
    hl->event_queue[hl->queue_tail].x = x;
    hl->event_queue[hl->queue_tail].y = y;
    hl->event_queue[hl->queue_tail].button = button;
    if (type == EVENT_MOUSE_BUTTON)
        hl->event_queue[hl->queue_tail].mouse_state = MOUSE_PRESS;
    else if (type == EVENT_MOUSE_MOTION)
        hl->event_queue[hl->queue_tail].mouse_state = MOUSE_MOTION;
    else if (type == EVENT_MOUSE_DRAG_START || type == EVENT_MOUSE_DRAG_MOVE) {
        hl->event_queue[hl->queue_tail].mouse_state = MOUSE_MOTION;
        hl->event_queue[hl->queue_tail].drag_dx = x;
        hl->event_queue[hl->queue_tail].drag_dy = y;
    }
    hl->queue_tail = next;
}

void headless_inject_resize(HeadlessBackend *hl, int w, int height) {
    hl->term_w = w; hl->term_h = height;
    int next = (hl->queue_tail + 1) % hl->queue_capacity;
    if (next == hl->queue_head) return;
    hl->event_queue[hl->queue_tail].type = EVENT_RESIZE;
    hl->event_queue[hl->queue_tail].w = w;
    hl->event_queue[hl->queue_tail].h = height;
    hl->queue_tail = next;
}

const char *headless_get_buffer(HeadlessBackend *hl) {
    return hl->buffer ? hl->buffer : "";
}

const uint32_t *headless_get_pixels(HeadlessBackend *hl) {
    return hl->pixels;
}