#include "../filly-core/backend.h"
#include "../filly-terminal/terminal.h"
extern BackendVTable terminal_vtable;
#include "daemon.h"
#include "../filly-protocol/protocol.h"
#include "../filly-terminal/terminal.h"
#include "../filly-terminal/renderer.h"
#include "../filly-core/session.h"
#include "../filly-core/widget.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <dirent.h>
#include <dlfcn.h>

typedef struct {
    int fd;
    int term_w, term_h;
} SocketBackend;

static bool sock_setup(void *self) { (void)self; return true; }

static bool sock_draw(void *self, RenderTree *tree) {
    SocketBackend *s = (SocketBackend *)self;
    int w = s->term_w, h = s->term_h;
    char buf[524288];
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

static bool sock_teardown(void *self) { (void)self; return true; }

static void sock_get_size(void *self, int *w, int *h) {
    SocketBackend *s = (SocketBackend *)self;
    *w = s->term_w; *h = s->term_h;
}

static BackendVTable socket_vtable = {
    .setup = sock_setup,
    .draw = sock_draw,
    .next_event = sock_next_event,
    .teardown = sock_teardown,
    .get_size = sock_get_size,
};

void load_plugins(void) {
    const char *home = getenv("HOME");
    if (!home) {
        fprintf(stderr, "load_plugins: HOME not set\n");
        return;
    }
    char path[1024];
    snprintf(path, sizeof(path), "%s/.config/filly/plugins", home);
    fprintf(stderr, "load_plugins: scanning %s\n", path);
    DIR *d = opendir(path);
    if (!d) {
        fprintf(stderr, "load_plugins: cannot open %s\n", path);
        return;
    }
    struct dirent *entry;
    while ((entry = readdir(d))) {
        int len = strlen(entry->d_name);
        if (len > 3 && strcmp(entry->d_name + len - 3, ".so") == 0) {
            char full[2048];
            snprintf(full, sizeof(full), "%s/%s", path, entry->d_name);
            fprintf(stderr, "load_plugins: loading %s\n", full);
            void *lib = dlopen(full, RTLD_NOW);
            if (!lib) {
                fprintf(stderr, "load_plugins: dlopen failed for %s: %s\n", full, dlerror());
                continue;
            }
            void (*reg)(void (*)(const char *, WidgetFactory));
            *(void **)(&reg) = dlsym(lib, "register_plugins");
            if (!reg) {
                fprintf(stderr, "load_plugins: dlsym failed for %s: %s\n", full, dlerror());
                dlclose(lib);
                continue;
            }
            reg(widget_registry_register);
            fprintf(stderr, "load_plugins: %s loaded successfully\n", full);
        }
    }
    closedir(d);
}

static void *handle_client(void *arg) {
    int fd = (intptr_t)arg;
    char buf[524288];
    bool term_ready = false, socket_mode = false;
    TerminalBackend t;
    SocketBackend sb = { .fd = fd, .term_w = 80, .term_h = 24 };
    Backend backend;

    while (1) {
        int n = 0;
        while (n < (int)sizeof(buf) - 1) {
            int r = read(fd, buf + n, 1);
            if (r <= 0) goto done;
            if (buf[n] == '\n') { buf[n] = '\0'; n++; break; }
            n++;
        }
        if (n == 0) break;

        WidgetRequest *req = widget_request_parse(buf);
        if (!req) {
            WidgetResponse resp = { .result = NULL, .cancelled = true, .error = "Invalid JSON" };
            char *json = widget_response_to_json(&resp);
            write(fd, json, strlen(json)); write(fd, "\n", 1);
            free(json);
            continue;
        }

        if (strcmp(req->widget, "quit") == 0) {
            WidgetResponse resp = { .result = NULL, .cancelled = false, .error = NULL };
            char *json = widget_response_to_json(&resp);
            write(fd, json, strlen(json)); write(fd, "\n", 1);
            free(json);
            widget_request_free(req);
            break;
        }

        socket_mode = req->relay;

        if (socket_mode) {
            backend.vtable = &socket_vtable;
            backend.data = &sb;
        } else {
            if (!term_ready) {
                if (!terminal_backend_init(&t, NULL)) {
                    WidgetResponse resp = { .result = NULL, .cancelled = true, .error = "No terminal" };
                    char *json = widget_response_to_json(&resp);
                    write(fd, json, strlen(json)); write(fd, "\n", 1);
                    free(json);
                    widget_request_free(req);
                    continue;
                }
                term_ready = true;
            }
            backend.vtable = &terminal_vtable;
            backend.data = &t;
        }

        Widget *w = widget_registry_create(req);
        WidgetResponse resp;
        if (w) {
            if (socket_mode) {
                char init[32];
                int l = snprintf(init, sizeof(init), "SIZE 80 24\n");
                write(fd, init, l);
            }
            resp = session_run(w, &backend);
            widget_destroy(w);
        } else {
            resp.result = NULL;
            resp.cancelled = true;
            resp.error = "Unknown widget";
        }

        char *json = widget_response_to_json(&resp);
        write(fd, json, strlen(json)); write(fd, "\n", 1);
        free(json);
        widget_request_free(req);

        if (socket_mode) break;
    }
done:
    if (term_ready && !socket_mode) terminal_backend_destroy(&t);
    close(fd);
    return NULL;
}

bool daemon_run(const char *socket_path) {
    load_plugins();
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return false;
    unlink(socket_path);
    struct sockaddr_un addr = { .sun_family = AF_UNIX };
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);
    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) { close(fd); return false; }
    if (listen(fd, 5) < 0) { close(fd); return false; }
    while (1) {
        int client = accept(fd, NULL, NULL);
        if (client < 0) continue;
        pthread_t tid;
        pthread_create(&tid, NULL, handle_client, (void *)(intptr_t)client);
        pthread_detach(tid);
    }
    close(fd);
    return true;
}