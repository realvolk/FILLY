#include "wayland-clipboard.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct WaylandClipboard {
    struct wl_display *display;
    struct wl_seat *seat;
    struct wl_data_device_manager *manager;
    struct wl_data_device *device;
    struct wl_data_source *source;
    struct wl_data_offer *offer;
    char *clipboard_text;
    uint32_t serial;
};

static void data_offer_offer(void *data, struct wl_data_offer *offer, const char *type) {
    (void)data; (void)offer; (void)type;
}

static const struct wl_data_offer_listener data_offer_listener = {
    .offer = data_offer_offer,
};

static void data_device_data_offer(void *data, struct wl_data_device *device, struct wl_data_offer *offer) {
    (void)data; (void)device; (void)offer;
    WaylandClipboard *wc = (WaylandClipboard *)data;
    wc->offer = offer;
    wl_data_offer_add_listener(offer, &data_offer_listener, wc);
}

static void data_device_selection(void *data, struct wl_data_device *device, struct wl_data_offer *offer) {
    (void)data; (void)device; (void)offer;
    WaylandClipboard *wc = (WaylandClipboard *)data;
    if (!offer) return;
    int fds[2];
    if (pipe(fds) == 0) {
        wl_data_offer_receive(offer, "text/plain", fds[1]);
        close(fds[1]);
        wl_display_flush(wc->display);
        wl_display_roundtrip(wc->display);
        char buf[8192];
        ssize_t n = read(fds[0], buf, sizeof(buf) - 1);
        close(fds[0]);
        if (n > 0) {
            buf[n] = '\0';
            free(wc->clipboard_text);
            wc->clipboard_text = strdup(buf);
        }
    }
}

static void data_device_enter(void *data, struct wl_data_device *device, uint32_t serial,
                              struct wl_surface *surface, wl_fixed_t x, wl_fixed_t y,
                              struct wl_data_offer *offer) {
    (void)data; (void)device; (void)serial; (void)surface; (void)x; (void)y; (void)offer;
}
static void data_device_leave(void *data, struct wl_data_device *device) { (void)data; (void)device; }
static void data_device_motion(void *data, struct wl_data_device *device, uint32_t time, wl_fixed_t x, wl_fixed_t y) {
    (void)data; (void)device; (void)time; (void)x; (void)y;
}
static void data_device_drop(void *data, struct wl_data_device *device) { (void)data; (void)device; }

static const struct wl_data_device_listener data_device_listener = {
    .data_offer = data_device_data_offer,
    .selection = data_device_selection,
    .enter = data_device_enter,
    .leave = data_device_leave,
    .motion = data_device_motion,
    .drop = data_device_drop,
};

WaylandClipboard *wayland_clipboard_create(struct wl_display *display, struct wl_seat *seat) {
    WaylandClipboard *wc = calloc(1, sizeof(WaylandClipboard));
    wc->display = display;
    wc->seat = seat;
    return wc;
}

void wayland_clipboard_bind_manager(WaylandClipboard *wc, struct wl_registry *registry, uint32_t id) {
    wc->manager = wl_registry_bind(registry, id, &wl_data_device_manager_interface, 3);
    wc->device = wl_data_device_manager_get_data_device(wc->manager, wc->seat);
    wl_data_device_add_listener(wc->device, &data_device_listener, wc);
}

void wayland_clipboard_set_serial(WaylandClipboard *wc, uint32_t serial) {
    wc->serial = serial;
}

bool wayland_clipboard_set(WaylandClipboard *wc, const char *text) {
    if (!wc || !wc->manager || !text) return false;
    struct wl_data_source *source = wl_data_device_manager_create_data_source(wc->manager);
    wl_data_source_offer(source, "text/plain");
    wl_data_device_set_selection(wc->device, source, wc->serial);
    wc->source = source;
    free(wc->clipboard_text);
    wc->clipboard_text = strdup(text);
    return true;
}

bool wayland_clipboard_has(WaylandClipboard *wc) {
    return wc && wc->clipboard_text != NULL;
}

char *wayland_clipboard_get(WaylandClipboard *wc) {
    if (!wc || !wc->clipboard_text) return NULL;
    return strdup(wc->clipboard_text);
}

void wayland_clipboard_destroy(WaylandClipboard *wc) {
    if (!wc) return;
    free(wc->clipboard_text);
    if (wc->source) wl_data_source_destroy(wc->source);
    if (wc->device) wl_data_device_destroy(wc->device);
    if (wc->manager) wl_data_device_manager_destroy(wc->manager);
    free(wc);
}