#pragma once
#include "core/backend.h"
#include "core/clipboard.h"
#include "gpu.h"
#include "renderer.h"
#include <stdbool.h>

#define EVENT_QUEUE_SIZE 128

typedef enum { GCORE_DRM, GCORE_X11, GCORE_WAYLAND } GCoreOutput;

typedef struct {
    GCoreOutput output;
    PixelBuffer pb;
    char *display_name;
    void *drm_data;
    void *li_data;
    Theme *theme;
    Arena *arena;
    RenderTree *render_tree;
    ClipboardInterface clipboard;
    int mouse_x, mouse_y;
    int mouse_button;
    Event event_queue[EVENT_QUEUE_SIZE];
    int queue_head, queue_tail;
    void *wl_clipboard;
    GPUBackend *gpu;
} GCoreBackend;

bool gcore_backend_init(GCoreBackend *g, GCoreOutput output, const char *display_name);
void gcore_backend_destroy(GCoreBackend *g);
bool gcore_backend_setup(void *self);
bool gcore_backend_draw(void *self, RenderTree *tree);
Event gcore_backend_next_event(void *self);
bool gcore_backend_teardown(void *self);
void gcore_backend_get_size(void *self, int *w, int *h);
extern BackendVTable gcore_vtable;