#include "socket_backend.h"
#include "../filly-core/render.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>

typedef struct {
    int fd;
    int term_w, term_h;
} SocketBackend;

static bool sock_setup(void *self) { return true; }

static bool sock_draw(void *self, RenderTree *tree) {
    SocketBackend *s = (SocketBackend *)self;
    int w = s->term_w, h = s->term_h;
    char buf[65536];
    render_tree_to_buf(tree, 0, 0, w, h, buf, sizeof(buf));
    int len = strlen(buf);
    char header[64];
    int hl = snprintf(header, sizeof(header), "DRAW %d\n", len);
    write(s->fd, header, hl);
    write(s->fd, buf, len);
    write(s->fd, "\n", 1);
    return true;
}

static Event sock_next_event(void *self) {
    SocketBackend *s = (SocketBackend *)self;
    char line[256];
    int i = 0;
    while (i < (int)sizeof(line)-1) {
        if (read(s->fd, line+i, 1) <= 0) { Event ev = {0}; return ev; }
        if (line[i] == '\n') { line[i] = '\0'; break; }
        i++;
    }
    Event ev = { .type = EVENT_NONE };
    if (strncmp(line, "KEY ", 4) == 0) {
        int code; char ch;
        if (sscanf(line+4, "%d %c", &code, &ch) == 2) {
            ev.type = EVENT_KEY;
            ev.code = (KeyCode)code;
            ev.ch = ch;
        } else if (sscanf(line+4, "%d", &code) == 1) {
            ev.type = EVENT_KEY;
            ev.code = (KeyCode)code;
            ev.ch = 0;
        }
    } else if (strncmp(line, "SIZE ", 5) == 0) {
        int w, h;
        if (sscanf(line+5, "%d %d", &w, &h) == 2) {
            s->term_w = w; s->term_h = h;
            ev.type = EVENT_RESIZE; ev.w = w; ev.h = h;
        }
    }
    return ev;
}

static bool sock_teardown(void *self) { return true; }

static void sock_get_size(void *self, int *w, int *h) {
    SocketBackend *s = (SocketBackend *)self;
    *w = s->term_w; *h = s->term_h;
}

static BackendVTable vtable = {
    .setup = sock_setup,
    .draw = sock_draw,
    .next_event = sock_next_event,
    .teardown = sock_teardown,
    .get_size = sock_get_size,
};

BackendVTable *socket_backend_vtable(void) { return &vtable; }