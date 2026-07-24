/* xdg-shell.c — Minimal xdg-shell + xdg-decoration interface definitions
 * -----------------------------------------------------------------------------
 * Provides the wl_interface symbols that wayland.c references via extern.
 * These are the minimum required for wl_registry_bind to resolve interfaces.
 * -----------------------------------------------------------------------------
 */
#include "xdg-shell.h"

/* Forward-declare interfaces before defining them (mutually recursive) */
extern const struct wl_interface wl_surface_interface;
extern const struct wl_interface wl_seat_interface;

const struct wl_interface xdg_wm_base_interface = {
    "xdg_wm_base", 1, 0, NULL, 0, NULL,
};

const struct wl_interface xdg_surface_interface = {
    "xdg_surface", 1, 0, NULL, 0, NULL,
};

const struct wl_interface xdg_toplevel_interface = {
    "xdg_toplevel", 1, 0, NULL, 0, NULL,
};

const struct wl_interface zxdg_decoration_manager_v1_interface = {
    "zxdg_decoration_manager_v1", 1, 0, NULL, 0, NULL,
};

const struct wl_interface zxdg_toplevel_decoration_v1_interface = {
    "zxdg_toplevel_decoration_v1", 1, 0, NULL, 0, NULL,
};