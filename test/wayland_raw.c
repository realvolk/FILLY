#include <wayland-client.h>
#include "xdg-shell-client-protocol.h"
#include <sys/mman.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <poll.h>
#include <stdbool.h>

static struct wl_compositor *compositor = NULL;
static struct wl_shm *shm = NULL;
static struct xdg_wm_base *wm_base = NULL;
static struct wl_surface *surface = NULL;
static struct xdg_surface *xdg_surface = NULL;
static struct xdg_toplevel *xdg_toplevel = NULL;
static struct wl_buffer *buffer = NULL;
static void *shm_data = NULL;
static int width = 800, height = 600;
static bool configured = false;
static bool running = true;

static void wm_base_ping(void *data, struct xdg_wm_base *xb, uint32_t serial) { xdg_wm_base_pong(xb, serial); }
static const struct xdg_wm_base_listener wm_base_listener = { .ping = wm_base_ping };

static void registry_global(void *data, struct wl_registry *r, uint32_t name, const char *iface, uint32_t ver) {
    if (strcmp(iface, wl_compositor_interface.name) == 0) compositor = wl_registry_bind(r, name, &wl_compositor_interface, 3);
    else if (strcmp(iface, wl_shm_interface.name) == 0) shm = wl_registry_bind(r, name, &wl_shm_interface, 1);
    else if (strcmp(iface, xdg_wm_base_interface.name) == 0) wm_base = wl_registry_bind(r, name, &xdg_wm_base_interface, 1);
}
static void registry_global_remove(void *data, struct wl_registry *r, uint32_t name) {}
static const struct wl_registry_listener registry_listener = { .global = registry_global, .global_remove = registry_global_remove };

static void xdg_surface_configure(void *data, struct xdg_surface *s, uint32_t serial) {
    xdg_surface_ack_configure(s, serial);
}
static const struct xdg_surface_listener xdg_surface_listener = { .configure = xdg_surface_configure };

static void xdg_toplevel_configure(void *data, struct xdg_toplevel *t, int32_t w, int32_t h, struct wl_array *states) {
    if (w > 0 && h > 0) { width = w; height = h; }
    configured = true;
}
static void xdg_toplevel_close(void *data, struct xdg_toplevel *t) { running = false; }
static const struct xdg_toplevel_listener xdg_toplevel_listener = { .configure = xdg_toplevel_configure, .close = xdg_toplevel_close };

static void create_buffer(void) {
    int stride = width * 4, size = stride * height;
    int fd = memfd_create("wayland-test", 0);
    ftruncate(fd, size);
    shm_data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    struct wl_shm_pool *pool = wl_shm_create_pool(shm, fd, size);
    buffer = wl_shm_pool_create_buffer(pool, 0, width, height, stride, WL_SHM_FORMAT_ARGB8888);
    wl_shm_pool_destroy(pool);
    close(fd);

    uint32_t *pixels = shm_data;
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            uint8_t r = (x * 255) / width;
            uint8_t g = (y * 255) / height;
            uint8_t b = 128;
            pixels[y * width + x] = (0xFF << 24) | (r << 16) | (g << 8) | b;
        }
    }
}

static void draw(void) {
    wl_surface_attach(surface, buffer, 0, 0);
    wl_surface_damage(surface, 0, 0, width, height);
    wl_surface_commit(surface);
}

int main(void) {
    struct wl_display *display = wl_display_connect(NULL);
    if (!display) { fprintf(stderr, "Cannot connect to Wayland\n"); return 1; }

    struct wl_registry *registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &registry_listener, NULL);
    wl_display_roundtrip(display);

    if (!compositor || !shm || !wm_base) { fprintf(stderr, "Missing globals\n"); return 1; }
    xdg_wm_base_add_listener(wm_base, &wm_base_listener, NULL);

    surface = wl_compositor_create_surface(compositor);
    xdg_surface = xdg_wm_base_get_xdg_surface(wm_base, surface);
    xdg_surface_add_listener(xdg_surface, &xdg_surface_listener, NULL);
    xdg_toplevel = xdg_surface_get_toplevel(xdg_surface);
    xdg_toplevel_add_listener(xdg_toplevel, &xdg_toplevel_listener, NULL);
    xdg_toplevel_set_title(xdg_toplevel, "Wayland Raw Test");
    xdg_toplevel_set_app_id(xdg_toplevel, "dev.filly.rawtest");

    wl_surface_commit(surface);
    wl_display_roundtrip(display);
    wl_display_roundtrip(display);

    if (!configured) { fprintf(stderr, "No configure\n"); return 1; }

    create_buffer();
    xdg_surface_set_window_geometry(xdg_surface, 0, 0, width, height);
    draw();

    fprintf(stderr, "Window should be visible. Press Ctrl+C to exit.\n");
    while (running) {
        wl_display_dispatch(display);
    }

    wl_buffer_destroy(buffer);
    munmap(shm_data, width * height * 4);
    xdg_toplevel_destroy(xdg_toplevel);
    xdg_surface_destroy(xdg_surface);
    wl_surface_destroy(surface);
    xdg_wm_base_destroy(wm_base);
    wl_compositor_destroy(compositor);
    wl_shm_destroy(shm);
    wl_registry_destroy(registry);
    wl_display_disconnect(display);
    return 0;
}