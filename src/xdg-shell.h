/* xdg-shell.h — Minimal xdg-shell + xdg-decoration protocol bindings for FILLY
 * -----------------------------------------------------------------------------
 * Hand-rolled from the Wayland XML protocol definitions.
 * Only the interfaces FILLY actually uses.
 * No generated code, no wayland-scanner dependency.
 * -----------------------------------------------------------------------------
 */
#ifndef FILLY_XDG_SHELL_H
#define FILLY_XDG_SHELL_H

#include <wayland-client.h>

/* ── xdg_wm_base ───────────────────────────────────────────────────────── */

#define XDG_WM_BASE_PONG 3
#define XDG_WM_BASE_GET_XDG_SURFACE 2

struct xdg_wm_base_listener {
    void (*ping)(void *data, struct xdg_wm_base *wm_base, uint32_t serial);
};

static inline struct xdg_surface *
xdg_wm_base_get_xdg_surface(struct xdg_wm_base *wm_base, struct wl_surface *surface) {
    struct wl_proxy *id = wl_proxy_marshal_flags(
        (struct wl_proxy *)wm_base, XDG_WM_BASE_GET_XDG_SURFACE,
        &xdg_surface_interface, wl_proxy_get_version((struct wl_proxy *)wm_base),
        0, NULL, surface);
    return (struct xdg_surface *)id;
}

static inline void
xdg_wm_base_pong(struct xdg_wm_base *wm_base, uint32_t serial) {
    wl_proxy_marshal_flags((struct wl_proxy *)wm_base, XDG_WM_BASE_PONG,
        NULL, wl_proxy_get_version((struct wl_proxy *)wm_base), 0, serial);
}

static inline int
xdg_wm_base_add_listener(struct xdg_wm_base *wm_base,
                          const struct xdg_wm_base_listener *listener, void *data) {
    return wl_proxy_add_listener((struct wl_proxy *)wm_base,
        (void (**)(void))listener, data);
}

/* ── xdg_surface ──────────────────────────────────────────────────────── */

#define XDG_SURFACE_GET_TOPLEVEL 1
#define XDG_SURFACE_ACK_CONFIGURE 4
#define XDG_SURFACE_SET_WINDOW_GEOMETRY 3

struct xdg_surface_listener {
    void (*configure)(void *data, struct xdg_surface *surface, uint32_t serial);
};

static inline struct xdg_toplevel *
xdg_surface_get_toplevel(struct xdg_surface *surface) {
    struct wl_proxy *id = wl_proxy_marshal_flags(
        (struct wl_proxy *)surface, XDG_SURFACE_GET_TOPLEVEL,
        &xdg_toplevel_interface, wl_proxy_get_version((struct wl_proxy *)surface),
        0, NULL);
    return (struct xdg_toplevel *)id;
}

static inline void
xdg_surface_ack_configure(struct xdg_surface *surface, uint32_t serial) {
    wl_proxy_marshal_flags((struct wl_proxy *)surface, XDG_SURFACE_ACK_CONFIGURE,
        NULL, wl_proxy_get_version((struct wl_proxy *)surface), 0, serial);
}

static inline void
xdg_surface_set_window_geometry(struct xdg_surface *surface,
                                 int32_t x, int32_t y, int32_t w, int32_t h) {
    wl_proxy_marshal_flags((struct wl_proxy *)surface,
        XDG_SURFACE_SET_WINDOW_GEOMETRY, NULL,
        wl_proxy_get_version((struct wl_proxy *)surface), 0, x, y, w, h);
}

static inline int
xdg_surface_add_listener(struct xdg_surface *surface,
                          const struct xdg_surface_listener *listener, void *data) {
    return wl_proxy_add_listener((struct wl_proxy *)surface,
        (void (**)(void))listener, data);
}

/* ── xdg_toplevel ─────────────────────────────────────────────────────── */

#define XDG_TOPLEVEL_SET_TITLE 2
#define XDG_TOPLEVEL_SET_APP_ID 3
#define XDG_TOPLEVEL_SET_MIN_SIZE 8

struct xdg_toplevel_listener {
    void (*configure)(void *data, struct xdg_toplevel *toplevel,
                      int32_t width, int32_t height, struct wl_array *states);
    void (*close)(void *data, struct xdg_toplevel *toplevel);
};

static inline void
xdg_toplevel_set_title(struct xdg_toplevel *toplevel, const char *title) {
    wl_proxy_marshal_flags((struct wl_proxy *)toplevel, XDG_TOPLEVEL_SET_TITLE,
        NULL, wl_proxy_get_version((struct wl_proxy *)toplevel), 0, title);
}

static inline void
xdg_toplevel_set_app_id(struct xdg_toplevel *toplevel, const char *app_id) {
    wl_proxy_marshal_flags((struct wl_proxy *)toplevel, XDG_TOPLEVEL_SET_APP_ID,
        NULL, wl_proxy_get_version((struct wl_proxy *)toplevel), 0, app_id);
}

static inline void
xdg_toplevel_set_min_size(struct xdg_toplevel *toplevel,
                           int32_t width, int32_t height) {
    wl_proxy_marshal_flags((struct wl_proxy *)toplevel, XDG_TOPLEVEL_SET_MIN_SIZE,
        NULL, wl_proxy_get_version((struct wl_proxy *)toplevel), 0, width, height);
}

static inline int
xdg_toplevel_add_listener(struct xdg_toplevel *toplevel,
                           const struct xdg_toplevel_listener *listener, void *data) {
    return wl_proxy_add_listener((struct wl_proxy *)toplevel,
        (void (**)(void))listener, data);
}

/* ── zxdg_decoration_manager_v1 ───────────────────────────────────────── */

#define ZXDG_TOPLEVEL_DECORATION_V1_SET_MODE 1
#define ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE 2

struct zxdg_toplevel_decoration_v1;

static inline struct zxdg_toplevel_decoration_v1 *
zxdg_decoration_manager_v1_get_toplevel_decoration(
    struct zxdg_decoration_manager_v1 *manager, struct xdg_toplevel *toplevel) {
    struct wl_proxy *id = wl_proxy_marshal_flags(
        (struct wl_proxy *)manager, 1, /* get_toplevel_decoration = opcode 1 */
        &zxdg_toplevel_decoration_v1_interface,
        wl_proxy_get_version((struct wl_proxy *)manager), 0, NULL, toplevel);
    return (struct zxdg_toplevel_decoration_v1 *)id;
}

static inline void
zxdg_toplevel_decoration_v1_set_mode(
    struct zxdg_toplevel_decoration_v1 *decoration, uint32_t mode) {
    wl_proxy_marshal_flags((struct wl_proxy *)decoration,
        ZXDG_TOPLEVEL_DECORATION_V1_SET_MODE, NULL,
        wl_proxy_get_version((struct wl_proxy *)decoration), 0, mode);
}

#endif /* FILLY_XDG_SHELL_H */