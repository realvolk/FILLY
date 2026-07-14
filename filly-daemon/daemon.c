#include "../filly-core/backend.h"
#include "../filly-terminal/terminal.h"
extern BackendVTable terminal_vtable;
#include "daemon.h"
#include "../filly-protocol/protocol.h"
#include "../filly-terminal/terminal.h"
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

static void load_plugins(void) {
    const char *home = getenv("HOME");
    if (!home) return;
    char path[1024];
    snprintf(path, sizeof(path), "%s/.config/filly/plugins", home);
    DIR *d = opendir(path);
    if (!d) return;
    struct dirent *entry;
    while ((entry = readdir(d))) {
        int len = strlen(entry->d_name);
        if (len > 3 && strcmp(entry->d_name + len - 3, ".so") == 0) {
            char full[2048];
            snprintf(full, sizeof(full), "%s/%s", path, entry->d_name);
            void *lib = dlopen(full, RTLD_NOW);
            if (lib) {
                void (*reg)(void (*)(const char *, WidgetFactory));
                *(void **)(&reg) = dlsym(lib, "register_plugins");
                if (reg) reg(widget_registry_register);
            }
        }
    }
    closedir(d);
}

static void *handle_client(void *arg) {
    int fd = (intptr_t)arg;
    TerminalBackend t;
    terminal_backend_init(&t);
    Backend backend = { .vtable = &terminal_vtable };

    char buf[524288];  // 512KB
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
        Widget *w = widget_registry_create(req);
        WidgetResponse resp;
        if (w) {
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
    }
done:
    terminal_backend_destroy(&t);
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