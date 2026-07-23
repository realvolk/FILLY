#pragma once
#include <wayland-client.h>
#include <stdbool.h>

typedef struct WaylandClipboard WaylandClipboard;

WaylandClipboard *wayland_clipboard_create(struct wl_display *display, struct wl_seat *seat);
void wayland_clipboard_bind_manager(WaylandClipboard *wc, struct wl_registry *registry, uint32_t id);
void wayland_clipboard_set_serial(WaylandClipboard *wc, uint32_t serial);
bool wayland_clipboard_set(WaylandClipboard *wc, const char *text);
bool wayland_clipboard_has(WaylandClipboard *wc);
char *wayland_clipboard_get(WaylandClipboard *wc);
void wayland_clipboard_destroy(WaylandClipboard *wc);