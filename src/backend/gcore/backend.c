#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "backend.h"
#include "core/session.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <drm.h>
#include <drm_mode.h>
#include <drm_fourcc.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <sys/mman.h>
#include <linux/input.h>
#include <libinput.h>
#include <dirent.h>
#include <sys/time.h>
#include <libudev.h>
#define KeyCode XKeyCode
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>
#undef KeyCode
#include "wayland-clipboard.h"

extern int setup_wayland(GCoreBackend *g);
extern void teardown_wayland(GCoreBackend *g);
extern void wayland_flush_draw(GCoreBackend *g);
extern Event wayland_next_event(GCoreBackend *g);

typedef struct {
    int fd;
    drmModeConnector *connector;
    drmModeEncoder *encoder;
    drmModeCrtc *crtc;
    drmModeModeInfo mode;
    uint32_t fb_id;
    struct drm_mode_create_dumb create_req;
    struct drm_mode_map_dumb map_req;
    uint32_t *buffer;
} DRMData;

typedef struct {
    struct libinput *li;
} LibInputData;

typedef struct {
    Display *dpy;
    Window win;
    GC gc;
    XImage *image;
    Atom wm_delete_window;
    bool running;
} X11Data;

static struct udev_monitor *drm_monitor = NULL;
static int drm_monitor_fd = -1;

static long long time_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long long)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

static uint32_t lerp_color(uint32_t c1, uint32_t c2, float t) {
    if (t >= 1.0f) return c2;
    if (t <= 0.0f) return c1;
    int r1 = (c1 >> 16) & 0xFF, g1 = (c1 >> 8) & 0xFF, b1 = c1 & 0xFF, a1 = (c1 >> 24) & 0xFF;
    int r2 = (c2 >> 16) & 0xFF, g2 = (c2 >> 8) & 0xFF, b2 = c2 & 0xFF, a2 = (c2 >> 24) & 0xFF;
    int r = r1 + (int)((r2 - r1) * t);
    int g = g1 + (int)((g2 - g1) * t);
    int b = b1 + (int)((b2 - b1) * t);
    int a = a1 + (int)((a2 - a1) * t);
    return (a << 24) | (r << 16) | (g << 8) | b;
}

/* window icon data: 32x32 ARGB */
static const unsigned long icon_data[] = {
    32, 32,
    0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,
    0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,
    0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,
    0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,
    0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,
    0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,
    0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,
    0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,
    0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,
    0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,
    0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,
    0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,
    0xFF1a1a2e,0xFF1a1a2e,0xFFe94560,0xFFe94560,0xFFe94560,0xFFe94560,0xFFe94560,0xFFe94560,
    0xFFe94560,0xFFe94560,0xFFe94560,0xFFe94560,0xFFe94560,0xFFe94560,0xFFe94560,0xFFe94560,
    0xFFe94560,0xFFe94560,0xFFe94560,0xFFe94560,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,
    0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,
    0xFF1a1a2e,0xFF1a1a2e,0xFFe94560,0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF,
    0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF,
    0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF,0xFFe94560,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,
    0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,
    0xFF1a1a2e,0xFF1a1a2e,0xFFe94560,0xFFFFFFFF,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,
    0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,
    0xFF1a1a2e,0xFF1a1a2e,0xFFFFFFFF,0xFFe94560,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,
    0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,
    0xFF1a1a2e,0xFF1a1a2e,0xFFe94560,0xFFFFFFFF,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,
    0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,
    0xFF1a1a2e,0xFF1a1a2e,0xFFFFFFFF,0xFFe94560,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,
    0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,
    0xFF1a1a2e,0xFF1a1a2e,0xFFe94560,0xFFFFFFFF,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,
    0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,
    0xFF1a1a2e,0xFF1a1a2e,0xFFFFFFFF,0xFFe94560,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,
    0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,
    0xFF1a1a2e,0xFF1a1a2e,0xFFe94560,0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF,
    0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF,
    0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF,0xFFe94560,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,
    0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,
    0xFF1a1a2e,0xFF1a1a2e,0xFFe94560,0xFFe94560,0xFFe94560,0xFFe94560,0xFFe94560,0xFFe94560,
    0xFFe94560,0xFFe94560,0xFFe94560,0xFFe94560,0xFFe94560,0xFFe94560,0xFFe94560,0xFFe94560,
    0xFFe94560,0xFFe94560,0xFFe94560,0xFFe94560,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,
    0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,
    0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,
    0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,
    0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,
    0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,
    0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,
    0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,
    0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,
    0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,
    0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,0xFF1a1a2e,
};

static int find_drm_device(void) {
    for (int i = 0; i < 16; i++) {
        char path[32];
        snprintf(path, sizeof(path), "/dev/dri/card%d", i);
        int fd = open(path, O_RDWR | O_CLOEXEC);
        if (fd < 0) continue;
        drmModeRes *res = drmModeGetResources(fd);
        if (res) {
            for (int j = 0; j < res->count_connectors; j++) {
                drmModeConnector *c = drmModeGetConnector(fd, res->connectors[j]);
                if (c && c->connection == DRM_MODE_CONNECTED && c->count_modes > 0) {
                    drmModeFreeConnector(c);
                    drmModeFreeResources(res);
                    return fd;
                }
                if (c) drmModeFreeConnector(c);
            }
            drmModeFreeResources(res);
        }
        close(fd);
    }
    return -1;
}

static int setup_drm(GCoreBackend *g) {
    DRMData *d = calloc(1, sizeof(DRMData));
    d->fd = find_drm_device();
    if (d->fd < 0) { free(d); return -1; }
    drmModeRes *res = drmModeGetResources(d->fd);
    if (!res) { close(d->fd); free(d); return -1; }
    for (int i = 0; i < res->count_connectors; i++) {
        d->connector = drmModeGetConnector(d->fd, res->connectors[i]);
        if (d->connector && d->connector->connection == DRM_MODE_CONNECTED && d->connector->count_modes > 0) {
            d->mode = d->connector->modes[0];
            break;
        }
        if (d->connector) { drmModeFreeConnector(d->connector); d->connector = NULL; }
    }
    drmModeFreeResources(res);
    if (!d->connector) { close(d->fd); free(d); return -1; }
    d->encoder = drmModeGetEncoder(d->fd, d->connector->encoder_id);
    if (!d->encoder) { drmModeFreeConnector(d->connector); close(d->fd); free(d); return -1; }
    d->crtc = drmModeGetCrtc(d->fd, d->encoder->crtc_id);
    if (!d->crtc) { drmModeFreeEncoder(d->encoder); drmModeFreeConnector(d->connector); close(d->fd); free(d); return -1; }
    g->pb.width = d->mode.hdisplay;
    g->pb.height = d->mode.vdisplay;
    g->pb.stride = d->mode.hdisplay;
    d->create_req.width = g->pb.width; d->create_req.height = g->pb.height; d->create_req.bpp = 32;
    if (drmIoctl(d->fd, DRM_IOCTL_MODE_CREATE_DUMB, &d->create_req) < 0) {
        drmModeFreeCrtc(d->crtc); drmModeFreeEncoder(d->encoder);
        drmModeFreeConnector(d->connector); close(d->fd); free(d); return -1;
    }
    d->map_req.handle = d->create_req.handle;
    if (drmIoctl(d->fd, DRM_IOCTL_MODE_MAP_DUMB, &d->map_req) < 0) {
        drmModeFreeCrtc(d->crtc); drmModeFreeEncoder(d->encoder);
        drmModeFreeConnector(d->connector); close(d->fd); free(d); return -1;
    }
    d->buffer = mmap(0, d->create_req.size, PROT_READ | PROT_WRITE, MAP_SHARED, d->fd, d->map_req.offset);
    if (d->buffer == MAP_FAILED) {
        drmModeFreeCrtc(d->crtc); drmModeFreeEncoder(d->encoder);
        drmModeFreeConnector(d->connector); close(d->fd); free(d); return -1;
    }
    g->pb.pixels = d->buffer;
    uint32_t handles[4] = {d->create_req.handle, d->create_req.pitch, 0, 0};
    uint32_t pitches[4] = {d->create_req.pitch, 0, 0, 0};
    uint32_t offsets[4] = {0, 0, 0, 0};
    if (drmModeAddFB2(d->fd, g->pb.width, g->pb.height, DRM_FORMAT_XRGB8888, handles, pitches, offsets, &d->fb_id, 0) < 0) d->fb_id = 0;
    if (drmModeSetCrtc(d->fd, d->crtc->crtc_id, d->fb_id, 0, 0, &d->connector->connector_id, 1, &d->mode) < 0) {
        munmap(d->buffer, d->create_req.size);
        if (d->fb_id) drmModeRmFB(d->fd, d->fb_id);
        drmModeFreeCrtc(d->crtc); drmModeFreeEncoder(d->encoder);
        drmModeFreeConnector(d->connector); close(d->fd); free(d); return -1;
    }
    g->drm_data = d;
    return 0;
}

static int setup_x11(GCoreBackend *g) {
    X11Data *x = calloc(1, sizeof(X11Data));
    x->dpy = XOpenDisplay(g->display_name);
    if (!x->dpy) { free(x); return -1; }
    int screen = DefaultScreen(x->dpy);
    x->win = XCreateSimpleWindow(x->dpy, RootWindow(x->dpy, screen), 0, 0, g->pb.width, g->pb.height, 1,
        BlackPixel(x->dpy, screen), WhitePixel(x->dpy, screen));
    x->wm_delete_window = XInternAtom(x->dpy, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(x->dpy, x->win, &x->wm_delete_window, 1);
    XSelectInput(x->dpy, x->win, ExposureMask | KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask | StructureNotifyMask);
    XMapWindow(x->dpy, x->win);
    x->gc = XCreateGC(x->dpy, x->win, 0, NULL);
    g->pb.pixels = calloc(g->pb.width * g->pb.height, sizeof(uint32_t));
    x->image = XCreateImage(x->dpy, DefaultVisual(x->dpy, screen), 24, ZPixmap, 0, (char *)g->pb.pixels, g->pb.width, g->pb.height, 32, 0);
    x->running = true;

    Atom net_wm_icon = XInternAtom(x->dpy, "_NET_WM_ICON", False);
    XChangeProperty(x->dpy, x->win, net_wm_icon, XA_CARDINAL, 32,
                    PropModeReplace, (unsigned char *)icon_data, 2 + 32*32);

    g->li_data = x;
    g->drm_data = NULL;
    return 0;
}

static int open_restricted(const char *path, int flags, void *user_data) { (void)user_data; return open(path, flags); }
static void close_restricted(int fd, void *user_data) { (void)user_data; close(fd); }
static const struct libinput_interface li_iface = { .open_restricted = open_restricted, .close_restricted = close_restricted };

static int setup_libinput(GCoreBackend *g) {
    LibInputData *li = calloc(1, sizeof(LibInputData));
    li->li = libinput_udev_create_context(&li_iface, NULL, NULL);
    if (!li->li) { free(li); return -1; }
    libinput_udev_assign_seat(li->li, "seat0");
    g->li_data = li;
    return 0;
}

static void setup_hotplug(GCoreBackend *g) {
    (void)g;
    struct udev *udev = udev_new();
    if (!udev) return;
    drm_monitor = udev_monitor_new_from_netlink(udev, "udev");
    udev_monitor_filter_add_match_subsystem_devtype(drm_monitor, "drm", NULL);
    udev_monitor_enable_receiving(drm_monitor);
    drm_monitor_fd = udev_monitor_get_fd(drm_monitor);
}

static void check_hotplug(GCoreBackend *g) {
    if (drm_monitor_fd < 0) return;
    fd_set fds; FD_ZERO(&fds); FD_SET(drm_monitor_fd, &fds);
    struct timeval tv = {0, 0};
    if (select(drm_monitor_fd+1, &fds, NULL, NULL, &tv) > 0) {
        struct udev_device *dev = udev_monitor_receive_device(drm_monitor);
        if (dev) {
            const char *action = udev_device_get_action(dev);
            if (strcmp(action, "add") == 0 || strcmp(action, "change") == 0) {
                DRMData *d = g->drm_data;
                if (d) {
                    munmap(d->buffer, d->create_req.size);
                    if (d->fb_id) drmModeRmFB(d->fd, d->fb_id);
                    drmModeFreeCrtc(d->crtc);
                    drmModeFreeEncoder(d->encoder);
                    drmModeFreeConnector(d->connector);
                    close(d->fd);
                    free(d);
                }
                setup_drm(g);
            }
            udev_device_unref(dev);
        }
    }
}

static void enqueue_event(GCoreBackend *g, Event ev) {
    int next = (g->queue_tail + 1) % EVENT_QUEUE_SIZE;
    if (next == g->queue_head) return;
    g->event_queue[g->queue_tail] = ev;
    g->queue_tail = next;
}

static Event dequeue_event(GCoreBackend *g) {
    if (g->queue_head == g->queue_tail) {
        Event ev = { .type = EVENT_NONE };
        return ev;
    }
    Event ev = g->event_queue[g->queue_head];
    g->queue_head = (g->queue_head + 1) % EVENT_QUEUE_SIZE;
    return ev;
}

bool gcore_backend_init(GCoreBackend *g, GCoreOutput output, const char *display_name) {
    memset(g, 0, sizeof(*g));
    g->display_name = display_name ? strdup(display_name) : NULL;
    g->theme = theme_load("themes/forge.json");
    if (!g->theme) g->theme = theme_load_directory("themes");
    if (!g->theme) g->theme = theme_default();
    g->arena = arena_new(1024 * 1024);
    g->render_tree = NULL;
    g->pb.width = 800; g->pb.height = 600;
    g->output = output;
    g->queue_head = 0;
    g->queue_tail = 0;
    g->wl_clipboard = NULL;
    gcore_init_font(NULL, 14);
    if (getenv("WAYLAND_DISPLAY")) { if (setup_wayland(g) == 0) { g->output = GCORE_WAYLAND; return true; } }
    if (getenv("DISPLAY")) { if (setup_x11(g) == 0) { g->output = GCORE_X11; return true; } }
    if (setup_drm(g) == 0) {
        g->output = GCORE_DRM;
        if (setup_libinput(g) < 0) {
            DRMData *d = g->drm_data;
            if (d) { munmap(d->buffer, d->create_req.size); if (d->fb_id) drmModeRmFB(d->fd, d->fb_id);
                drmModeFreeCrtc(d->crtc); drmModeFreeEncoder(d->encoder); drmModeFreeConnector(d->connector); close(d->fd); free(d); }
            return false;
        }
        setup_hotplug(g);
        return true;
    }
    return false;
}

static void free_tree_copy(RenderTree *t) {
    if (!t) return;
    if (t->type == RNODE_CONTAINER && t->container.children)
        free(t->container.children);
    free(t);
}

static RenderTree *copy_tree_malloc(RenderTree *src) {
    if (!src) return NULL;
    RenderTree *dst = malloc(sizeof(RenderTree));
    memcpy(dst, src, sizeof(RenderTree));
    if (src->type == RNODE_CONTAINER && src->container.child_count > 0) {
        dst->container.children = malloc(src->container.child_count * sizeof(RenderTree));
        for (int i = 0; i < src->container.child_count; i++) {
            RenderTree *child = copy_tree_malloc(&src->container.children[i]);
            memcpy(&dst->container.children[i], child, sizeof(RenderTree));
            free(child);
        }
    } else {
        dst->container.children = NULL;
    }
    return dst;
}

void gcore_backend_destroy(GCoreBackend *g) {
    if (!g) return;
    if (g->render_tree) free_tree_copy(g->render_tree);
    if (g->output == GCORE_DRM) {
        LibInputData *li = g->li_data; if (li && li->li) libinput_unref(li->li); free(li);
        DRMData *d = g->drm_data;
        if (d) { munmap(d->buffer, d->create_req.size); if (d->fb_id) drmModeRmFB(d->fd, d->fb_id);
            drmModeFreeCrtc(d->crtc); drmModeFreeEncoder(d->encoder); drmModeFreeConnector(d->connector); close(d->fd); free(d); }
        g->pb.pixels = NULL;
    } else if (g->output == GCORE_X11) {
        X11Data *x = g->li_data;
        if (x) {
            if (x->image) XDestroyImage(x->image);
            if (x->gc) XFreeGC(x->dpy, x->gc);
            if (x->win) XDestroyWindow(x->dpy, x->win);
            if (x->dpy) XCloseDisplay(x->dpy);
            free(x);
        }
        free(g->pb.pixels);
        g->pb.pixels = NULL;
    } else if (g->output == GCORE_WAYLAND) { teardown_wayland(g); }
    free(g->display_name); if (g->theme) theme_free(g->theme); if (g->arena) arena_free(g->arena);
    if (g->wl_clipboard) wayland_clipboard_destroy(g->wl_clipboard);
}

bool gcore_backend_setup(void *self) { (void)self; return true; }

bool gcore_backend_draw(void *self, RenderTree *tree) {
    GCoreBackend *g = (GCoreBackend *)self;
    arena_reset(g->arena);
    for (int row = 0; row < g->pb.height; row++) {
        uint32_t *line = g->pb.pixels + row * g->pb.width;
        for (int col = 0; col < g->pb.width; col++)
            line[col] = rgba(26, 26, 46, 255);
    }
    g->pb.mouse_x = g->mouse_x;
    g->pb.mouse_y = g->mouse_y;

    if (tree && tree->resolved_style.transition_ms > 0 && tree->state_change_time > 0) {
        long long now = time_ms();
        WidgetStyle *ws = &tree->resolved_style;
        long long elapsed = now - tree->state_change_time;
        float t = (float)elapsed / ws->transition_ms;
        if (t >= 1.0f) t = 1.0f;
        t = t < 0.5f ? 2.0f * t * t : -1.0f + (4.0f - 2.0f * t) * t;
        ws->bg_color = lerp_color(tree->prev_resolved_style.bg_color, ws->bg_color, t);
        ws->fg_color = lerp_color(tree->prev_resolved_style.fg_color, ws->fg_color, t);
        ws->border_color = lerp_color(tree->prev_resolved_style.border_color, ws->border_color, t);
        ws->accent_color = lerp_color(tree->prev_resolved_style.accent_color, ws->accent_color, t);
        ws->opacity = tree->prev_resolved_style.opacity + (ws->opacity - tree->prev_resolved_style.opacity) * t;
        ws->border_radius = (int)(tree->prev_resolved_style.border_radius + (ws->border_radius - tree->prev_resolved_style.border_radius) * t);
        ws->font_size = (int)(tree->prev_resolved_style.font_size + (ws->font_size - tree->prev_resolved_style.font_size) * t);
    }

    gcore_render_tree_to_pixels(tree, &g->pb, g->theme, g->arena);
#ifdef FILLY_PROFILING
    {
        extern double session_current_fps;
        if (session_current_fps > 0 && gcore_font_loaded) {
            char buf[32];
            snprintf(buf, sizeof(buf), "FPS: %.0f", session_current_fps);
            draw_text_pixel(&g->pb, g->pb.width - 80, 5, buf, 12, 0xFF00FF00);
        }
    }
#endif
    if (g->render_tree) free_tree_copy(g->render_tree);
    g->render_tree = copy_tree_malloc(tree);
    if (g->output == GCORE_DRM) {
        DRMData *d = g->drm_data;
        if (d) drmModePageFlip(d->fd, d->crtc->crtc_id, d->fb_id, DRM_MODE_PAGE_FLIP_EVENT, NULL);
    } else if (g->output == GCORE_X11) {
        X11Data *x = g->li_data;
        if (x && x->image) { XPutImage(x->dpy, x->win, x->gc, x->image, 0, 0, 0, 0, g->pb.width, g->pb.height); XFlush(x->dpy); }
    } else if (g->output == GCORE_WAYLAND) { wayland_flush_draw(g); }
    return true;
}

static RenderTree *hit_test(RenderTree *node, int px, int py, int off_x, int off_y, int *abs_x, int *abs_y) {
    if (!node) return NULL;
    int x = off_x + node->rect.x, y = off_y + node->rect.y, w = node->rect.w, h = node->rect.h;
    if (px < x || px >= x + w || py < y || py >= y + h) return NULL;
    if (node->type == RNODE_CONTAINER && node->container.children) {
        for (int i = node->container.child_count - 1; i >= 0; i--) {
            int cx, cy;
            RenderTree *hit = hit_test(&node->container.children[i], px, py, x, y, &cx, &cy);
            if (hit) { if (abs_x) *abs_x = cx; if (abs_y) *abs_y = cy; return hit; }
        }
    }
    if (abs_x) *abs_x = x;
    if (abs_y) *abs_y = y;
    return node;
}

static void synthesize_key_events(GCoreBackend *g, RenderTree *hit, int px, int py, int abs_x, int abs_y) {
    (void)px;
    (void)abs_x;
    if (!hit) return;
    if (hit->type == RNODE_LIST && hit->list.item_count > 0) {
        WidgetStyle *s = &hit->resolved_style;
        int item_h = s->font_size + s->padding[0] + s->padding[2];
        if (item_h <= 0) item_h = 20;
        int rel_y = py - abs_y - s->margin[0] - s->padding[0] - s->border_width;
        int idx = rel_y / item_h;
        if (idx < 0) idx = 0;
        if (idx >= hit->list.item_count) idx = hit->list.item_count - 1;
        int current = hit->list.selected;
        if (idx < current) {
            for (int i = 0; i < current - idx; i++)
                enqueue_event(g, (Event){ .type = EVENT_KEY, .code = KEY_UP });
        } else if (idx > current) {
            for (int i = 0; i < idx - current; i++)
                enqueue_event(g, (Event){ .type = EVENT_KEY, .code = KEY_DOWN });
        }
        enqueue_event(g, (Event){ .type = EVENT_KEY, .code = KEY_ENTER });
    } else if (hit->type == RNODE_INPUT) {
        enqueue_event(g, (Event){ .type = EVENT_KEY, .code = KEY_ENTER });
    } else if (hit->type == RNODE_TOGGLE || hit->type == RNODE_CHECKBOX) {
        enqueue_event(g, (Event){ .type = EVENT_KEY, .code = KEY_CHAR, .ch = ' ' });
    } else if (hit->type == RNODE_TEXT) {
        enqueue_event(g, (Event){ .type = EVENT_KEY, .code = KEY_ENTER });
    }
}

Event gcore_backend_next_event(void *self) {
    GCoreBackend *g = (GCoreBackend *)self;
    Event queued = dequeue_event(g);
    if (queued.type != EVENT_NONE) return queued;

    if (g->output == GCORE_DRM) check_hotplug(g);

    Event ev = { .type = EVENT_NONE };
    if (g->output == GCORE_DRM) {
        LibInputData *li = g->li_data;
        if (!li || !li->li) return ev;
        int li_fd = libinput_get_fd(li->li);
        fd_set fds; FD_ZERO(&fds); FD_SET(li_fd, &fds);
        if (select(li_fd+1, &fds, NULL, NULL, NULL) <= 0) return ev;
        libinput_dispatch(li->li);
        struct libinput_event *event = libinput_get_event(li->li);
        if (!event) return ev;
        int type = libinput_event_get_type(event);
        switch (type) {
        case LIBINPUT_EVENT_KEYBOARD_KEY: {
            struct libinput_event_keyboard *k = libinput_event_get_keyboard_event(event);
            uint32_t key = libinput_event_keyboard_get_key(k);
            uint32_t state = libinput_event_keyboard_get_key_state(k);
            if (state == LIBINPUT_KEY_STATE_PRESSED) {
                ev.type = EVENT_KEY;
                if (key == KEY_ENTER) ev.code = KEY_ENTER;
                else if (key == KEY_ESC) ev.code = KEY_ESC;
                else if (key == KEY_UP) ev.code = KEY_UP;
                else if (key == KEY_DOWN) ev.code = KEY_DOWN;
                else if (key == KEY_LEFT) ev.code = KEY_LEFT;
                else if (key == KEY_RIGHT) ev.code = KEY_RIGHT;
                else if (key == KEY_TAB) ev.code = KEY_TAB;
                else if (key == KEY_BACKSPACE) ev.code = KEY_BACKSPACE;
                else if (key >= KEY_F1 && key <= KEY_F12) ev.code = KEY_F1 + (key - KEY_F1);
                else if (key >= 1 && key <= 127) { ev.code = KEY_CHAR; ev.ch = key; }
            }
            break;
        }
        case LIBINPUT_EVENT_POINTER_MOTION: {
            struct libinput_event_pointer *p = libinput_event_get_pointer_event(event);
            g->mouse_x += libinput_event_pointer_get_dx(p);
            g->mouse_y += libinput_event_pointer_get_dy(p);
            if (g->mouse_x < 0) g->mouse_x = 0;
            if (g->mouse_y < 0) g->mouse_y = 0;
            if (g->mouse_x >= g->pb.width) g->mouse_x = g->pb.width - 1;
            if (g->mouse_y >= g->pb.height) g->mouse_y = g->pb.height - 1;
            ev.type = EVENT_MOUSE_MOTION; ev.x = g->mouse_x; ev.y = g->mouse_y; ev.mouse_state = MOUSE_MOTION;
            break;
        }
        case LIBINPUT_EVENT_POINTER_BUTTON: {
            struct libinput_event_pointer *p = libinput_event_get_pointer_event(event);
            uint32_t state = libinput_event_pointer_get_button_state(p);
            if (state == LIBINPUT_BUTTON_STATE_PRESSED) {
                int abs_x = 0, abs_y = 0;
                RenderTree *hit = hit_test(g->render_tree, g->mouse_x, g->mouse_y, 0, 0, &abs_x, &abs_y);
                synthesize_key_events(g, hit, g->mouse_x, g->mouse_y, abs_x, abs_y);
                return dequeue_event(g);
            }
            break;
        }
        case LIBINPUT_EVENT_POINTER_SCROLL_WHEEL: {
            struct libinput_event_pointer *p = libinput_event_get_pointer_event(event);
            ev.type = EVENT_MOUSE_SCROLL; ev.x = g->mouse_x; ev.y = g->mouse_y;
            ev.mouse_state = libinput_event_pointer_get_scroll_value(p, LIBINPUT_POINTER_AXIS_SCROLL_VERTICAL) < 0 ? MOUSE_SCROLL_UP : MOUSE_SCROLL_DOWN;
            break;
        }
        }
        libinput_event_destroy(event);
    } else if (g->output == GCORE_X11) {
        X11Data *x = g->li_data;
        if (!x || !x->dpy) return ev;
        if (!XPending(x->dpy)) return ev;
        XEvent xe; XNextEvent(x->dpy, &xe);
        if (xe.type == KeyPress) {
            KeySym ks = XLookupKeysym(&xe.xkey, 0);
            unsigned int mods = xe.xkey.state;
            if ((mods & ControlMask) && ks == XK_c) {
                extern ClipboardInterface *session_clipboard;
                if (session_clipboard && session_clipboard->has_clipboard(session_clipboard->data)) {
                    char *text = session_clipboard->get_clipboard(session_clipboard->data);
                    if (text) {
                        XSetSelectionOwner(x->dpy, XA_PRIMARY, x->win, CurrentTime);
                        Atom utf8 = XInternAtom(x->dpy, "UTF8_STRING", False);
                        XChangeProperty(x->dpy, x->win, XA_PRIMARY, utf8, 8, PropModeReplace, (unsigned char *)text, strlen(text));
                        free(text);
                    }
                }
                ev.type = EVENT_NONE;
            } else if ((mods & ControlMask) && ks == XK_v) {
                Atom utf8 = XInternAtom(x->dpy, "UTF8_STRING", False);
                Atom property = XInternAtom(x->dpy, "FILLY_PROP", False);
                XConvertSelection(x->dpy, XA_PRIMARY, utf8, property, x->win, CurrentTime);
                XFlush(x->dpy);
                XEvent se;
                while (1) {
                    XNextEvent(x->dpy, &se);
                    if (se.type == SelectionNotify && se.xselection.property != None) {
                        Atom type; int format; unsigned long nitems, bytes_after;
                        unsigned char *data = NULL;
                        XGetWindowProperty(x->dpy, x->win, property, 0, 1024, False, AnyPropertyType,
                                           &type, &format, &nitems, &bytes_after, &data);
                        if (data) {
                            for (unsigned long i = 0; i < nitems; i++) {
                                enqueue_event(g, (Event){ .type = EVENT_KEY, .code = KEY_CHAR, .ch = ((char*)data)[i] });
                            }
                            XFree(data);
                            XDeleteProperty(x->dpy, x->win, property);
                        }
                        break;
                    }
                }
                ev.type = EVENT_NONE;
            } else {
                ev.type = EVENT_KEY;
                if (ks == XK_Return) ev.code = KEY_ENTER;
                else if (ks == XK_Escape) ev.code = KEY_ESC;
                else if (ks == XK_Up) ev.code = KEY_UP;
                else if (ks == XK_Down) ev.code = KEY_DOWN;
                else if (ks == XK_Left) ev.code = KEY_LEFT;
                else if (ks == XK_Right) ev.code = KEY_RIGHT;
                else if (ks == XK_Tab) ev.code = KEY_TAB;
                else if (ks == XK_BackSpace) ev.code = KEY_BACKSPACE;
                else if (ks >= XK_F1 && ks <= XK_F12) ev.code = KEY_F1 + (ks - XK_F1);
                else if (ks >= 32 && ks <= 126) { ev.code = KEY_CHAR; ev.ch = ks; }
            }
        } else if (xe.type == ButtonPress) {
            g->mouse_x = xe.xbutton.x; g->mouse_y = xe.xbutton.y;
            int abs_x = 0, abs_y = 0;
            RenderTree *hit = hit_test(g->render_tree, g->mouse_x, g->mouse_y, 0, 0, &abs_x, &abs_y);
            synthesize_key_events(g, hit, g->mouse_x, g->mouse_y, abs_x, abs_y);
            return dequeue_event(g);
        } else if (xe.type == MotionNotify) {
            g->mouse_x = xe.xmotion.x; g->mouse_y = xe.xmotion.y;
            ev.type = EVENT_MOUSE_MOTION; ev.x = g->mouse_x; ev.y = g->mouse_y; ev.mouse_state = MOUSE_MOTION;
        } else if (xe.type == ClientMessage && (Atom)xe.xclient.data.l[0] == x->wm_delete_window) {
            ev.type = EVENT_KEY; ev.code = KEY_ESC;
        } else if (xe.type == ConfigureNotify) {
            XConfigureEvent *ce = (XConfigureEvent*)&xe;
            if (ce->width != g->pb.width || ce->height != g->pb.height) {
                g->pb.width = ce->width;
                g->pb.height = ce->height;
                free(g->pb.pixels);
                g->pb.pixels = calloc(g->pb.width * g->pb.height, sizeof(uint32_t));
                x->image->data = (char *)g->pb.pixels;
                x->image->width = g->pb.width;
                x->image->height = g->pb.height;
                ev.type = EVENT_RESIZE;
                ev.w = g->pb.width;
                ev.h = g->pb.height;
            }
        }
    } else if (g->output == GCORE_WAYLAND) {
        int old_w = g->pb.width, old_h = g->pb.height;
        ev = wayland_next_event(g);
        if (g->pb.width != old_w || g->pb.height != old_h) {
            ev.type = EVENT_RESIZE; ev.w = g->pb.width; ev.h = g->pb.height;
            return ev;
        }
        if (ev.type == EVENT_MOUSE_BUTTON && ev.mouse_state == MOUSE_PRESS) {
            g->mouse_x = ev.x; g->mouse_y = ev.y;
            int abs_x = 0, abs_y = 0;
            RenderTree *hit = hit_test(g->render_tree, ev.x, ev.y, 0, 0, &abs_x, &abs_y);
            synthesize_key_events(g, hit, ev.x, ev.y, abs_x, abs_y);
            return dequeue_event(g);
        }
        if (ev.type == EVENT_MOUSE_MOTION) {
            g->mouse_x = ev.x; g->mouse_y = ev.y;
        }
        return ev;
    }
    return ev;
}

static void gcore_backend_wait_frame(void *self) {
    GCoreBackend *g = (GCoreBackend *)self;
    if (g->output == GCORE_DRM) {
    } else if (g->output == GCORE_X11) {
        X11Data *x = g->li_data;
        if (x && x->dpy) XSync(x->dpy, False);
    } else if (g->output == GCORE_WAYLAND) {
    }
}

bool gcore_backend_teardown(void *self) { (void)self; return true; }
void gcore_backend_get_size(void *self, int *w, int *h) { GCoreBackend *g = (GCoreBackend *)self; *w = g->pb.width; *h = g->pb.height; }

BackendVTable gcore_vtable = {
    .setup = gcore_backend_setup, .draw = gcore_backend_draw, .next_event = gcore_backend_next_event,
    .teardown = gcore_backend_teardown, .get_size = gcore_backend_get_size,
    .wait_frame = gcore_backend_wait_frame, .is_interactive = true,
};