#ifndef FILLY_XDG_SHELL_H
#define FILLY_XDG_SHELL_H

#include <stdint.h>
#include <stddef.h>
#include "wayland-client.h"

#ifdef  __cplusplus
extern "C" {
#endif

struct wl_output;
struct wl_seat;
struct wl_surface;
struct xdg_popup;
struct xdg_positioner;
struct xdg_surface;
struct xdg_toplevel;
struct xdg_wm_base;
struct zxdg_decoration_manager_v1;
struct zxdg_toplevel_decoration_v1;

extern const struct wl_interface xdg_wm_base_interface;
extern const struct wl_interface xdg_positioner_interface;
extern const struct wl_interface xdg_surface_interface;
extern const struct wl_interface xdg_toplevel_interface;
extern const struct wl_interface xdg_popup_interface;
extern const struct wl_interface zxdg_decoration_manager_v1_interface;
extern const struct wl_interface zxdg_toplevel_decoration_v1_interface;

struct xdg_wm_base_listener {
    void (*ping)(void *data, struct xdg_wm_base *xdg_wm_base, uint32_t serial);
};

static inline int
xdg_wm_base_add_listener(struct xdg_wm_base *xdg_wm_base,
                         const struct xdg_wm_base_listener *listener, void *data)
{
    return wl_proxy_add_listener((struct wl_proxy *) xdg_wm_base,
                                 (void (**)(void)) listener, data);
}

#define XDG_WM_BASE_DESTROY 0
#define XDG_WM_BASE_GET_XDG_SURFACE 2
#define XDG_WM_BASE_PONG 3

static inline void
xdg_wm_base_destroy(struct xdg_wm_base *xdg_wm_base)
{
    wl_proxy_marshal_flags((struct wl_proxy *) xdg_wm_base,
                           XDG_WM_BASE_DESTROY, NULL,
                           wl_proxy_get_version((struct wl_proxy *) xdg_wm_base),
                           WL_MARSHAL_FLAG_DESTROY);
}

static inline struct xdg_surface *
xdg_wm_base_get_xdg_surface(struct xdg_wm_base *xdg_wm_base, struct wl_surface *surface)
{
    struct wl_proxy *id;
    id = wl_proxy_marshal_flags((struct wl_proxy *) xdg_wm_base,
                                 XDG_WM_BASE_GET_XDG_SURFACE, &xdg_surface_interface,
                                 wl_proxy_get_version((struct wl_proxy *) xdg_wm_base),
                                 0, NULL, surface);
    return (struct xdg_surface *) id;
}

static inline void
xdg_wm_base_pong(struct xdg_wm_base *xdg_wm_base, uint32_t serial)
{
    wl_proxy_marshal_flags((struct wl_proxy *) xdg_wm_base,
                           XDG_WM_BASE_PONG, NULL,
                           wl_proxy_get_version((struct wl_proxy *) xdg_wm_base),
                           0, serial);
}

struct xdg_surface_listener {
    void (*configure)(void *data, struct xdg_surface *xdg_surface, uint32_t serial);
};

static inline int
xdg_surface_add_listener(struct xdg_surface *xdg_surface,
                         const struct xdg_surface_listener *listener, void *data)
{
    return wl_proxy_add_listener((struct wl_proxy *) xdg_surface,
                                 (void (**)(void)) listener, data);
}

#define XDG_SURFACE_DESTROY 0
#define XDG_SURFACE_GET_TOPLEVEL 1
#define XDG_SURFACE_SET_WINDOW_GEOMETRY 3
#define XDG_SURFACE_ACK_CONFIGURE 4

static inline void
xdg_surface_destroy(struct xdg_surface *xdg_surface)
{
    wl_proxy_marshal_flags((struct wl_proxy *) xdg_surface,
                           XDG_SURFACE_DESTROY, NULL,
                           wl_proxy_get_version((struct wl_proxy *) xdg_surface),
                           WL_MARSHAL_FLAG_DESTROY);
}

static inline struct xdg_toplevel *
xdg_surface_get_toplevel(struct xdg_surface *xdg_surface)
{
    struct wl_proxy *id;
    id = wl_proxy_marshal_flags((struct wl_proxy *) xdg_surface,
                                 XDG_SURFACE_GET_TOPLEVEL, &xdg_toplevel_interface,
                                 wl_proxy_get_version((struct wl_proxy *) xdg_surface),
                                 0, NULL);
    return (struct xdg_toplevel *) id;
}

static inline void
xdg_surface_set_window_geometry(struct xdg_surface *xdg_surface,
                                 int32_t x, int32_t y, int32_t w, int32_t h)
{
    wl_proxy_marshal_flags((struct wl_proxy *) xdg_surface,
                           XDG_SURFACE_SET_WINDOW_GEOMETRY, NULL,
                           wl_proxy_get_version((struct wl_proxy *) xdg_surface),
                           0, x, y, w, h);
}

static inline void
xdg_surface_ack_configure(struct xdg_surface *xdg_surface, uint32_t serial)
{
    wl_proxy_marshal_flags((struct wl_proxy *) xdg_surface,
                           XDG_SURFACE_ACK_CONFIGURE, NULL,
                           wl_proxy_get_version((struct wl_proxy *) xdg_surface),
                           0, serial);
}

struct xdg_toplevel_listener {
    void (*configure)(void *data, struct xdg_toplevel *xdg_toplevel,
                      int32_t width, int32_t height, struct wl_array *states);
    void (*close)(void *data, struct xdg_toplevel *xdg_toplevel);
    void (*configure_bounds)(void *data, struct xdg_toplevel *xdg_toplevel,
                             int32_t width, int32_t height);
    void (*wm_capabilities)(void *data, struct xdg_toplevel *xdg_toplevel,
                            struct wl_array *capabilities);
};

static inline int
xdg_toplevel_add_listener(struct xdg_toplevel *xdg_toplevel,
                          const struct xdg_toplevel_listener *listener, void *data)
{
    return wl_proxy_add_listener((struct wl_proxy *) xdg_toplevel,
                                 (void (**)(void)) listener, data);
}

#define XDG_TOPLEVEL_DESTROY 0
#define XDG_TOPLEVEL_SET_TITLE 2
#define XDG_TOPLEVEL_SET_APP_ID 3
#define XDG_TOPLEVEL_SET_MIN_SIZE 8

static inline void
xdg_toplevel_destroy(struct xdg_toplevel *xdg_toplevel)
{
    wl_proxy_marshal_flags((struct wl_proxy *) xdg_toplevel,
                           XDG_TOPLEVEL_DESTROY, NULL,
                           wl_proxy_get_version((struct wl_proxy *) xdg_toplevel),
                           WL_MARSHAL_FLAG_DESTROY);
}

static inline void
xdg_toplevel_set_title(struct xdg_toplevel *xdg_toplevel, const char *title)
{
    wl_proxy_marshal_flags((struct wl_proxy *) xdg_toplevel,
                           XDG_TOPLEVEL_SET_TITLE, NULL,
                           wl_proxy_get_version((struct wl_proxy *) xdg_toplevel),
                           0, title);
}

static inline void
xdg_toplevel_set_app_id(struct xdg_toplevel *xdg_toplevel, const char *app_id)
{
    wl_proxy_marshal_flags((struct wl_proxy *) xdg_toplevel,
                           XDG_TOPLEVEL_SET_APP_ID, NULL,
                           wl_proxy_get_version((struct wl_proxy *) xdg_toplevel),
                           0, app_id);
}

static inline void
xdg_toplevel_set_min_size(struct xdg_toplevel *xdg_toplevel,
                           int32_t width, int32_t height)
{
    wl_proxy_marshal_flags((struct wl_proxy *) xdg_toplevel,
                           XDG_TOPLEVEL_SET_MIN_SIZE, NULL,
                           wl_proxy_get_version((struct wl_proxy *) xdg_toplevel),
                           0, width, height);
}

#define ZXDG_DECORATION_MANAGER_V1_DESTROY 0
#define ZXDG_DECORATION_MANAGER_V1_GET_TOPLEVEL_DECORATION 1

static inline void
zxdg_decoration_manager_v1_destroy(struct zxdg_decoration_manager_v1 *mgr)
{
    wl_proxy_marshal_flags((struct wl_proxy *) mgr,
                           ZXDG_DECORATION_MANAGER_V1_DESTROY, NULL,
                           wl_proxy_get_version((struct wl_proxy *) mgr),
                           WL_MARSHAL_FLAG_DESTROY);
}

static inline struct zxdg_toplevel_decoration_v1 *
zxdg_decoration_manager_v1_get_toplevel_decoration(
    struct zxdg_decoration_manager_v1 *mgr, struct xdg_toplevel *toplevel)
{
    struct wl_proxy *id;
    id = wl_proxy_marshal_flags((struct wl_proxy *) mgr,
                                 ZXDG_DECORATION_MANAGER_V1_GET_TOPLEVEL_DECORATION,
                                 &zxdg_toplevel_decoration_v1_interface,
                                 wl_proxy_get_version((struct wl_proxy *) mgr),
                                 0, NULL, toplevel);
    return (struct zxdg_toplevel_decoration_v1 *) id;
}

#define ZXDG_TOPLEVEL_DECORATION_V1_DESTROY 0
#define ZXDG_TOPLEVEL_DECORATION_V1_SET_MODE 1
#define ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE 2

static inline void
zxdg_toplevel_decoration_v1_destroy(struct zxdg_toplevel_decoration_v1 *dec)
{
    wl_proxy_marshal_flags((struct wl_proxy *) dec,
                           ZXDG_TOPLEVEL_DECORATION_V1_DESTROY, NULL,
                           wl_proxy_get_version((struct wl_proxy *) dec),
                           WL_MARSHAL_FLAG_DESTROY);
}

static inline void
zxdg_toplevel_decoration_v1_set_mode(struct zxdg_toplevel_decoration_v1 *dec,
                                      uint32_t mode)
{
    wl_proxy_marshal_flags((struct wl_proxy *) dec,
                           ZXDG_TOPLEVEL_DECORATION_V1_SET_MODE, NULL,
                           wl_proxy_get_version((struct wl_proxy *) dec),
                           0, mode);
}

#ifdef  __cplusplus
}
#endif

#endif