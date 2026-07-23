#include "xdg-shell-client-protocol.h"
#include "xdg-decoration-client-protocol.h"
#include "backend.h"
#include <xkbcommon/xkbcommon.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include "wayland-clipboard.h"

typedef struct {
    struct wl_display *display;
    struct wl_registry *registry;
    struct wl_compositor *compositor;
    struct wl_shm *shm;
    struct wl_shm_pool *pool;
    struct wl_buffer *buffer;
    struct wl_surface *surface;
    struct xdg_wm_base *wm_base;
    struct xdg_surface *xdg_surface;
    struct xdg_toplevel *xdg_toplevel;
    struct zxdg_decoration_manager_v1 *decoration_manager;
    struct zxdg_toplevel_decoration_v1 *decoration;
    struct wl_seat *seat;
    struct wl_pointer *pointer;
    struct wl_keyboard *keyboard;
    struct xkb_context *xkb_ctx;
    struct xkb_keymap *xkb_keymap;
    struct xkb_state *xkb_state;
    void *shm_data;
    int shm_size;
    int width, height;
    bool configured;
    bool running;
    int mouse_x, mouse_y;
    uint32_t mouse_button;
    bool mouse_pressed;
    int pending_key;
    bool key_available;
    uint32_t serial;
    GCoreBackend *backend;
} WaylandData;

static int create_shm_buffer(WaylandData *w, int width, int height) {
    int stride = width * 4, size = stride * height;
    if (w->buffer) { wl_buffer_destroy(w->buffer); w->buffer = NULL; }
    if (w->pool) { wl_shm_pool_destroy(w->pool); w->pool = NULL; }
    if (w->shm_data) { munmap(w->shm_data, w->shm_size); w->shm_data = NULL; }
    int fd = memfd_create("filly-wayland-shm", 0);
    if (fd < 0) return -1;
    if (ftruncate(fd, size) < 0) { close(fd); return -1; }
    w->shm_data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (w->shm_data == MAP_FAILED) { close(fd); return -1; }
    w->shm_size = size;
    w->pool = wl_shm_create_pool(w->shm, fd, size);
    w->buffer = wl_shm_pool_create_buffer(w->pool, 0, width, height, stride, WL_SHM_FORMAT_XRGB8888);
    close(fd);
    return 0;
}

static const struct wl_pointer_listener pointer_listener;
static const struct wl_keyboard_listener keyboard_listener;

static void wm_base_ping(void *data, struct xdg_wm_base *xb, uint32_t serial) { (void)data; xdg_wm_base_pong(xb, serial); }
static const struct xdg_wm_base_listener wm_base_listener = { .ping = wm_base_ping };

static void wl_seat_capabilities(void *data, struct wl_seat *seat, uint32_t caps) {
    WaylandData *w = (WaylandData *)data;
    if (caps & WL_SEAT_CAPABILITY_POINTER && !w->pointer) { w->pointer = wl_seat_get_pointer(seat); wl_pointer_add_listener(w->pointer, &pointer_listener, w); }
    if (caps & WL_SEAT_CAPABILITY_KEYBOARD && !w->keyboard) { w->keyboard = wl_seat_get_keyboard(seat); wl_keyboard_add_listener(w->keyboard, &keyboard_listener, w); }
}
static void wl_seat_name(void *data, struct wl_seat *seat, const char *name) { (void)data; (void)seat; (void)name; }
static const struct wl_seat_listener seat_listener = { .capabilities = wl_seat_capabilities, .name = wl_seat_name };

static void wl_registry_global(void *data, struct wl_registry *r, uint32_t name, const char *iface, uint32_t ver) {
    (void)ver; WaylandData *w = (WaylandData *)data;
    if (strcmp(iface, wl_compositor_interface.name) == 0) w->compositor = wl_registry_bind(r, name, &wl_compositor_interface, 3);
    else if (strcmp(iface, wl_shm_interface.name) == 0) w->shm = wl_registry_bind(r, name, &wl_shm_interface, 1);
    else if (strcmp(iface, xdg_wm_base_interface.name) == 0) w->wm_base = wl_registry_bind(r, name, &xdg_wm_base_interface, 1);
    else if (strcmp(iface, "zxdg_decoration_manager_v1") == 0) w->decoration_manager = wl_registry_bind(r, name, &zxdg_decoration_manager_v1_interface, 1);
    else if (strcmp(iface, wl_seat_interface.name) == 0) { w->seat = wl_registry_bind(r, name, &wl_seat_interface, 5); wl_seat_add_listener(w->seat, &seat_listener, w); }
    else if (strcmp(iface, "wl_data_device_manager") == 0) {
        GCoreBackend *g = w->backend;
        if (!g->wl_clipboard) {
            g->wl_clipboard = wayland_clipboard_create(w->display, w->seat);
        }
        wayland_clipboard_bind_manager(g->wl_clipboard, r, name);
    }
}
static void wl_registry_global_remove(void *data, struct wl_registry *r, uint32_t name) { (void)data; (void)r; (void)name; }
static const struct wl_registry_listener registry_listener = { .global = wl_registry_global, .global_remove = wl_registry_global_remove };

static void wl_xdg_surface_configure(void *data, struct xdg_surface *s, uint32_t serial) {
    xdg_surface_ack_configure(s, serial);
    wl_display_flush(((WaylandData *)data)->display);
}
static const struct xdg_surface_listener xdg_surface_listener = { .configure = wl_xdg_surface_configure };

static void wl_xdg_toplevel_configure(void *data, struct xdg_toplevel *t, int32_t w, int32_t h, struct wl_array *states) {
    (void)t; (void)states; WaylandData *wd = (WaylandData *)data;
    if (w <= 0) w = 800;
    if (h <= 0) h = 600;
    if (w != wd->width || h != wd->height || !wd->configured) {
        wd->width = w; wd->height = h;
        if (wd->configured) {
            create_shm_buffer(wd, w, h);
            wd->backend->pb.pixels = wd->shm_data;
            wd->backend->pb.width = w;
            wd->backend->pb.height = h;
        }
    }
    wd->configured = true;
}
static void wl_xdg_toplevel_close(void *data, struct xdg_toplevel *t) { (void)t; ((WaylandData *)data)->running = false; }
static void wl_xdg_toplevel_configure_bounds(void *d, struct xdg_toplevel *t, int32_t w, int32_t h) { (void)d; (void)t; (void)w; (void)h; }
static void wl_xdg_toplevel_wm_capabilities(void *d, struct xdg_toplevel *t, struct wl_array *c) { (void)d; (void)t; (void)c; }
static const struct xdg_toplevel_listener xdg_toplevel_listener = {
    .configure = wl_xdg_toplevel_configure, .close = wl_xdg_toplevel_close,
    .configure_bounds = wl_xdg_toplevel_configure_bounds, .wm_capabilities = wl_xdg_toplevel_wm_capabilities
};

static void wl_pointer_enter(void *d, struct wl_pointer *p, uint32_t s, struct wl_surface *sf, wl_fixed_t sx, wl_fixed_t sy) {
    (void)p; (void)sf; WaylandData *w = (WaylandData *)d;
    w->mouse_x = wl_fixed_to_int(sx); w->mouse_y = wl_fixed_to_int(sy);
    w->serial = s;
}
static void wl_pointer_leave(void *d, struct wl_pointer *p, uint32_t s, struct wl_surface *sf) { (void)d; (void)p; (void)s; (void)sf; }
static void wl_pointer_motion(void *d, struct wl_pointer *p, uint32_t t, wl_fixed_t sx, wl_fixed_t sy) { (void)p; (void)t; WaylandData *w = (WaylandData *)d; w->mouse_x = wl_fixed_to_int(sx); w->mouse_y = wl_fixed_to_int(sy); }
static void wl_pointer_button(void *d, struct wl_pointer *p, uint32_t s, uint32_t t, uint32_t b, uint32_t st) { (void)p; (void)t; WaylandData *w = (WaylandData *)d; w->mouse_button = b; w->mouse_pressed = (st == WL_POINTER_BUTTON_STATE_PRESSED); w->serial = s; }
static void wl_pointer_axis(void *d, struct wl_pointer *p, uint32_t t, uint32_t a, wl_fixed_t v) { (void)d; (void)p; (void)t; (void)a; (void)v; }
static void wl_pointer_frame(void *d, struct wl_pointer *p) { (void)d; (void)p; }
static void wl_pointer_axis_source(void *d, struct wl_pointer *p, uint32_t s) { (void)d; (void)p; (void)s; }
static void wl_pointer_axis_stop(void *d, struct wl_pointer *p, uint32_t t, uint32_t a) { (void)d; (void)p; (void)t; (void)a; }
static void wl_pointer_axis_discrete(void *d, struct wl_pointer *p, uint32_t a, int32_t v) { (void)d; (void)p; (void)a; (void)v; }
static void wl_pointer_axis_value120(void *d, struct wl_pointer *p, uint32_t a, int32_t v) { (void)d; (void)p; (void)a; (void)v; }
static const struct wl_pointer_listener pointer_listener = {
    .enter = wl_pointer_enter, .leave = wl_pointer_leave, .motion = wl_pointer_motion,
    .button = wl_pointer_button, .axis = wl_pointer_axis, .frame = wl_pointer_frame,
    .axis_source = wl_pointer_axis_source, .axis_stop = wl_pointer_axis_stop,
    .axis_discrete = wl_pointer_axis_discrete, .axis_value120 = wl_pointer_axis_value120
};

static void wl_keyboard_keymap(void *d, struct wl_keyboard *k, uint32_t f, int fd, uint32_t s) {
    (void)k; WaylandData *w = (WaylandData *)d;
    if (f == WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1) {
        char *m = mmap(NULL, s, PROT_READ, MAP_PRIVATE, fd, 0);
        if (m != MAP_FAILED) {
            w->xkb_keymap = xkb_keymap_new_from_string(w->xkb_ctx, m, XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS);
            munmap(m, s);
            if (w->xkb_state) xkb_state_unref(w->xkb_state);
            w->xkb_state = xkb_state_new(w->xkb_keymap);
        }
        close(fd);
    }
}
static void wl_keyboard_enter(void *d, struct wl_keyboard *k, uint32_t s, struct wl_surface *sf, struct wl_array *keys) { (void)k; (void)sf; (void)keys; ((WaylandData *)d)->serial = s; }
static void wl_keyboard_leave(void *d, struct wl_keyboard *k, uint32_t s, struct wl_surface *sf) { (void)d; (void)k; (void)s; (void)sf; }
static void wl_keyboard_key(void *d, struct wl_keyboard *k, uint32_t s, uint32_t t, uint32_t key, uint32_t st) {
    (void)k; (void)t; WaylandData *w = (WaylandData *)d;
    if (st == WL_KEYBOARD_KEY_STATE_PRESSED && w->xkb_state) {
        w->pending_key = xkb_state_key_get_one_sym(w->xkb_state, key + 8);
        w->key_available = true;
    }
    w->serial = s;
}
static void wl_keyboard_modifiers(void *d, struct wl_keyboard *k, uint32_t s, uint32_t dep, uint32_t lat, uint32_t lock, uint32_t grp) {
    (void)k; (void)s; WaylandData *w = (WaylandData *)d;
    if (w->xkb_state) xkb_state_update_mask(w->xkb_state, dep, lat, lock, 0, 0, grp);
}
static void wl_keyboard_repeat_info(void *d, struct wl_keyboard *k, int32_t r, int32_t dl) { (void)d; (void)k; (void)r; (void)dl; }
static const struct wl_keyboard_listener keyboard_listener = {
    .keymap = wl_keyboard_keymap, .enter = wl_keyboard_enter, .leave = wl_keyboard_leave,
    .key = wl_keyboard_key, .modifiers = wl_keyboard_modifiers, .repeat_info = wl_keyboard_repeat_info
};

int setup_wayland(GCoreBackend *g) {
    WaylandData *w = calloc(1, sizeof(WaylandData));
    w->backend = g;
    w->xkb_ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    w->display = wl_display_connect(g->display_name);
    if (!w->display) { free(w); return -1; }
    w->registry = wl_display_get_registry(w->display);
    wl_registry_add_listener(w->registry, &registry_listener, w);
    wl_display_roundtrip(w->display);
    if (!w->compositor || !w->shm || !w->wm_base) { wl_display_disconnect(w->display); free(w); return -1; }
    xdg_wm_base_add_listener(w->wm_base, &wm_base_listener, w);
    w->surface = wl_compositor_create_surface(w->compositor);
    w->xdg_surface = xdg_wm_base_get_xdg_surface(w->wm_base, w->surface);
    xdg_surface_add_listener(w->xdg_surface, &xdg_surface_listener, w);
    w->xdg_toplevel = xdg_surface_get_toplevel(w->xdg_surface);
    xdg_toplevel_add_listener(w->xdg_toplevel, &xdg_toplevel_listener, w);
    xdg_toplevel_set_title(w->xdg_toplevel, "FILLY");
    xdg_toplevel_set_app_id(w->xdg_toplevel, "dev.filly.gui");
    xdg_toplevel_set_min_size(w->xdg_toplevel, 100, 100);
    if (w->decoration_manager) {
        w->decoration = zxdg_decoration_manager_v1_get_toplevel_decoration(w->decoration_manager, w->xdg_toplevel);
        zxdg_toplevel_decoration_v1_set_mode(w->decoration, ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
    }
    w->configured = false;
    w->width = 800; w->height = 600;
    wl_surface_commit(w->surface);
    wl_display_roundtrip(w->display);
    wl_display_roundtrip(w->display);
    if (!w->configured) { wl_display_disconnect(w->display); free(w); return -1; }
    create_shm_buffer(w, w->width, w->height);
    g->pb.pixels = w->shm_data;
    g->pb.width = w->width;
    g->pb.height = w->height;
    for (int row = 0; row < g->pb.height; row++) {
        uint32_t *line = g->pb.pixels + row * g->pb.width;
        for (int col = 0; col < g->pb.width; col++)
            line[col] = 0xFF1a1a2e;
    }
    xdg_surface_set_window_geometry(w->xdg_surface, 0, 0, w->width, w->height);
    wl_surface_attach(w->surface, w->buffer, 0, 0);
    wl_surface_damage(w->surface, 0, 0, w->width, w->height);
    wl_surface_commit(w->surface);
    wl_display_flush(w->display);
    wl_display_roundtrip(w->display);
    w->running = true;
    g->li_data = w;
    g->drm_data = NULL;
    return 0;
}

void teardown_wayland(GCoreBackend *g) {
    WaylandData *w = g->li_data;
    if (!w) return;
    if (w->decoration) zxdg_toplevel_decoration_v1_destroy(w->decoration);
    if (w->decoration_manager) zxdg_decoration_manager_v1_destroy(w->decoration_manager);
    if (w->display) wl_display_disconnect(w->display);
    g->pb.pixels = NULL;
    free(w);
    g->li_data = NULL;
}

void wayland_flush_draw(GCoreBackend *g) {
    WaylandData *w = g->li_data;
    if (!w || !w->buffer || !w->surface || !w->configured) return;
    xdg_surface_set_window_geometry(w->xdg_surface, 0, 0, w->width, w->height);
    wl_surface_attach(w->surface, w->buffer, 0, 0);
    wl_surface_damage(w->surface, 0, 0, w->width, w->height);
    wl_surface_commit(w->surface);
    wl_display_flush(w->display);
}

Event wayland_next_event(GCoreBackend *g) {
    Event ev = { .type = EVENT_NONE };
    WaylandData *w = g->li_data;
    if (!w || !w->display) return ev;
    if (!w->running) { ev.type = EVENT_KEY; ev.code = KEY_ESC; return ev; }
    wl_display_dispatch(w->display);
    if (!w->running) { ev.type = EVENT_KEY; ev.code = KEY_ESC; return ev; }
    if (w->key_available) {
        w->key_available = false;
        ev.type = EVENT_KEY;
        uint32_t ks = w->pending_key;
        if (ks == XKB_KEY_Return) ev.code = KEY_ENTER;
        else if (ks == XKB_KEY_Escape) ev.code = KEY_ESC;
        else if (ks == XKB_KEY_Up) ev.code = KEY_UP;
        else if (ks == XKB_KEY_Down) ev.code = KEY_DOWN;
        else if (ks == XKB_KEY_Left) ev.code = KEY_LEFT;
        else if (ks == XKB_KEY_Right) ev.code = KEY_RIGHT;
        else if (ks == XKB_KEY_Tab) ev.code = KEY_TAB;
        else if (ks == XKB_KEY_BackSpace) ev.code = KEY_BACKSPACE;
        else if (ks >= XKB_KEY_F1 && ks <= XKB_KEY_F12) ev.code = KEY_F1 + (ks - XKB_KEY_F1);
        else if (ks >= 32 && ks <= 126) { ev.code = KEY_CHAR; ev.ch = ks; }
    }
    if (w->mouse_pressed) {
        w->mouse_pressed = false;
        ev.type = EVENT_MOUSE_BUTTON;
        ev.x = w->mouse_x; ev.y = w->mouse_y;
        ev.button = w->mouse_button;
        ev.mouse_state = MOUSE_PRESS;
    }
    return ev;
}