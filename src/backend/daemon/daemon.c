#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "core/backend.h"
#include "backend/terminal/terminal.h"
extern BackendVTable terminal_vtable;
#include "daemon.h"
#include "checkpoint.h"
#include "verify.h"
#include "sandbox.h"
#include "protocol/protocol.h"
#include "protocol/schema.h"
#include "backend/terminal/terminal.h"
#include "backend/terminal/renderer.h"
#include "core/session.h"
#include "core/widget.h"
#include "backend/headless/headless.h"
#include "script/fil.h"
#include "core/clipboard.h"
#include "core/log.h"
#include "core/config.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>
#include <dirent.h>
#include <dlfcn.h>
#include <signal.h>
#include <time.h>
#ifdef FILLY_GCORE
#include "backend/gcore/backend.h"
#include "backend/gcore/wayland-clipboard.h"
#endif

#define INACTIVITY_TIMEOUT 30
#define MAX_SOCKET_BUFFER (1024 * 1024)

static bool insecure_plugins = false;
bool use_sandbox = false;

void set_insecure_plugins(bool val) { insecure_plugins = val; }
void set_use_sandbox(bool val) { use_sandbox = val; }

static volatile sig_atomic_t daemon_running = 1;
static void handle_signal(int sig) { (void)sig; daemon_running = 0; }

static bool check_peer_cred(int fd) {
    struct ucred cred;
    socklen_t len = sizeof(cred);
    if (getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &cred, &len) < 0) return false;
    return cred.uid == getuid();
}

static bool tty_is_owned_by_user(const char *path) {
    return access(path, R_OK | W_OK) == 0;
}

typedef struct {
    int fd;
    int tty_fd;
    int term_w, term_h;
    struct termios orig;
    bool tty_raw;
} SocketBackend;

static bool sock_setup(void *self) {
    SocketBackend *s = (SocketBackend *)self;
    if (s->tty_fd < 0) return false;
    tcgetattr(s->tty_fd, &s->orig);
    struct termios raw = s->orig;
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_iflag &= ~(IXON | ICRNL | BRKINT | INPCK | ISTRIP);
    raw.c_cflag |= CS8;
    raw.c_oflag &= ~OPOST;
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;
    tcsetattr(s->tty_fd, TCSAFLUSH, &raw);
    s->tty_raw = true;
    return true;
}

static bool sock_draw(void *self, RenderTree *tree) {
    SocketBackend *s = (SocketBackend *)self;
    int w = s->term_w, h = s->term_h;
    char buf[524288];
    render_tree_to_buf(tree, 0, 0, w, h, buf, sizeof(buf));
    int len = strlen(buf);
    char header[128];
    int hl = snprintf(header, sizeof(header), "{\"type\":\"draw\",\"len\":%d}\n", len);
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
    cJSON *msg = cJSON_Parse(line);
    if (!msg) return ev;
    cJSON *type = cJSON_GetObjectItem(msg, "type");
    if (type && type->valuestring && strcmp(type->valuestring, "key") == 0) {
        cJSON *code = cJSON_GetObjectItem(msg, "code");
        cJSON *ch = cJSON_GetObjectItem(msg, "ch");
        if (code) {
            ev.type = EVENT_KEY;
            ev.code = (KeyCode)code->valueint;
            ev.ch = (ch && ch->valuestring && strlen(ch->valuestring) > 0) ? ch->valuestring[0] : 0;
        }
    } else if (type && type->valuestring && strcmp(type->valuestring, "size") == 0) {
        cJSON *w = cJSON_GetObjectItem(msg, "w");
        cJSON *h = cJSON_GetObjectItem(msg, "h");
        if (w && h) { s->term_w = w->valueint; s->term_h = h->valueint; ev.type = EVENT_RESIZE; ev.w = w->valueint; ev.h = h->valueint; }
    } else if (type && type->valuestring && strcmp(type->valuestring, "mouse") == 0) {
        cJSON *x = cJSON_GetObjectItem(msg, "x");
        cJSON *y = cJSON_GetObjectItem(msg, "y");
        cJSON *btn = cJSON_GetObjectItem(msg, "button");
        cJSON *st = cJSON_GetObjectItem(msg, "state");
        if (x && y) { ev.type = EVENT_MOUSE_BUTTON; ev.x = x->valueint; ev.y = y->valueint; ev.button = btn ? btn->valueint : 0; if (st && st->valuestring) { if (strcmp(st->valuestring, "press") == 0) ev.mouse_state = MOUSE_PRESS; else if (strcmp(st->valuestring, "release") == 0) ev.mouse_state = MOUSE_RELEASE; } }
    }
    cJSON_Delete(msg);
    return ev;
}

static bool sock_teardown(void *self) {
    SocketBackend *s = (SocketBackend *)self;
    if (s->tty_raw && s->tty_fd >= 0) { tcsetattr(s->tty_fd, TCSAFLUSH, &s->orig); s->tty_raw = false; }
    return true;
}

static void sock_get_size(void *self, int *w, int *h) {
    SocketBackend *s = (SocketBackend *)self;
    *w = s->term_w; *h = s->term_h;
}

static BackendVTable socket_vtable = {
    .setup = sock_setup, .draw = sock_draw, .next_event = sock_next_event,
    .teardown = sock_teardown, .get_size = sock_get_size,
    .wait_frame = NULL, .is_interactive = true,
};

static void send_error(int fd, const char *error_msg) {
    char buf[512];
    snprintf(buf, sizeof(buf), "{\"type\":\"response\",\"result\":null,\"cancelled\":true,\"error\":\"%s\"}\n", error_msg);
    write(fd, buf, strlen(buf));
}

void load_plugins(void) {
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
            if (!insecure_plugins && !verify_plugin_signature(full)) { LOG_WARN("Plugin %s failed signature verification", full); continue; }
            if (use_sandbox) { continue; }
            void *lib = dlopen(full, RTLD_NOW);
            if (lib) {
                void (*reg)(void (*)(const char *, WidgetFactory));
                *(void **)(&reg) = dlsym(lib, "register_plugins");
                if (reg) { reg(widget_registry_register); LOG_INFO("Plugin loaded: %s", full); }
                else LOG_WARN("Plugin %s has no register_plugins", full);
            } else LOG_ERROR("dlopen failed for %s: %s", full, dlerror());
        }
    }
    closedir(d);
}

static Session *sessions = NULL;
static int session_count = 0;

static Session *session_find(const char *id) {
    for (int i = 0; i < session_count; i++)
        if (strcmp(sessions[i].id, id) == 0 && sessions[i].active) return &sessions[i];
    return NULL;
}

static Session *session_create(void) {
    sessions = realloc(sessions, (session_count + 1) * sizeof(Session));
    Session *s = &sessions[session_count++];
    snprintf(s->id, sizeof(s->id), "%lx%lx", (unsigned long)time(NULL), (unsigned long)pthread_self());
    s->store = store_new();
    s->active = true;
    return s;
}

static char *read_message(int fd, time_t *last_activity) {
    char *buf = malloc(524288);
    int n = 0;
    while (n < 524287) {
        int pending = 0;
        if (ioctl(fd, FIONREAD, &pending) == 0 && pending > MAX_SOCKET_BUFFER) { LOG_WARN("Client exceeded max socket buffer, disconnecting"); free(buf); return NULL; }
        fd_set fds; FD_ZERO(&fds); FD_SET(fd, &fds);
        struct timeval tv = { .tv_sec = 1, .tv_usec = 0 };
        int ret = select(fd + 1, &fds, NULL, NULL, &tv);
        if (ret < 0) { free(buf); return NULL; }
        if (ret == 0) {
            if (time(NULL) - *last_activity > INACTIVITY_TIMEOUT) { LOG_INFO("Client inactivity timeout"); free(buf); return NULL; }
            continue;
        }
        int r = read(fd, buf + n, 1);
        if (r <= 0) { free(buf); return NULL; }
        if (buf[n] == '\n') { buf[n] = '\0'; *last_activity = time(NULL); return buf; }
        n++;
    }
    free(buf);
    return NULL;
}

static const char *widget_schema =
    "{\"type\":\"object\",\"properties\":{\"widget\":{\"type\":\"string\"},\"params\":{\"type\":\"object\"}}}";

#ifdef FILLY_GCORE
static void *handle_gui_client(void *arg) {
    int fd = (intptr_t)arg;
    GCoreBackend *gcore = calloc(1, sizeof(GCoreBackend));
    if (!gcore_backend_init(gcore, GCORE_DRM, NULL)) { send_error(fd, "GUI backend unavailable"); close(fd); free(gcore); return NULL; }
    Backend backend = { .vtable = &gcore_vtable, .data = gcore };
    backend.vtable->setup(backend.data);
    Session *current_session = session_create();
    (void)current_session;
    time_t last_activity = time(NULL);
    bool running = true;
    while (running) {
        char *msg = read_message(fd, &last_activity);
        if (!msg) break;
        char *schema_error = NULL;
        if (!schema_validate(msg, widget_schema, &schema_error)) { LOG_WARN("Schema validation failed: %s", schema_error); send_error(fd, schema_error); free(schema_error); free(msg); continue; }
        cJSON *json = cJSON_Parse(msg);
        if (!json) { free(msg); continue; }
        cJSON *type = cJSON_GetObjectItem(json, "type");
        if (type && type->valuestring) {
            if (strcmp(type->valuestring, "quit") == 0) { cJSON_Delete(json); const char *resp = "{\"type\":\"response\",\"result\":null,\"cancelled\":false}\n"; write(fd, resp, strlen(resp)); running = false; break; }
            if (strcmp(type->valuestring, "ping") == 0) { write(fd, "{\"type\":\"pong\"}\n", 16); cJSON_Delete(json); free(msg); continue; }
            if (strcmp(type->valuestring, "subscribe") == 0) { cJSON *keys = cJSON_GetObjectItem(json, "keys"); if (keys && keys->type == cJSON_Array) { cJSON *key; cJSON_ArrayForEach(key, keys) if (key->valuestring) store_subscribe(current_session->store, key->valuestring, fd); } cJSON_Delete(json); free(msg); continue; }
        }
        WidgetRequest *req = widget_request_parse(msg);
        free(msg); cJSON_Delete(json);
        if (!req) { send_error(fd, "Invalid JSON"); continue; }
        Widget *w = widget_registry_create(req);
        WidgetResponse resp;
        if (w) { resp = session_run(w, &backend); widget_destroy(w); }
        else { resp.result = NULL; resp.cancelled = true; resp.error = "Unknown widget"; }
        char *response_json = widget_response_to_json(&resp);
        write(fd, response_json, strlen(response_json)); write(fd, "\n", 1);
        free(response_json); widget_request_free(req);
    }
    gcore_backend_destroy(gcore); free(gcore); close(fd);
    return NULL;
}
#endif

static void *handle_client(void *arg) {
    int fd = (intptr_t)arg;
    time_t last_activity = time(NULL);
    char *msg = read_message(fd, &last_activity);
    if (!msg) { close(fd); return NULL; }
    char *schema_error = NULL;
    if (!schema_validate(msg, widget_schema, &schema_error)) { LOG_WARN("Schema validation failed: %s", schema_error); send_error(fd, schema_error); free(schema_error); free(msg); close(fd); return NULL; }
    cJSON *first = cJSON_Parse(msg);
    bool gui_req = false;
    if (first) { cJSON *gui = cJSON_GetObjectItem(first, "gui"); if (gui) gui_req = gui->valueint; cJSON_Delete(first); }
    free(msg);
#ifdef FILLY_GCORE
    if (gui_req) { handle_gui_client(arg); return NULL; }
#endif
    bool term_ready = false, socket_mode = false;
    TerminalBackend t;
    HeadlessBackend hl;
    SocketBackend sb = { .fd = fd, .tty_fd = -1, .term_w = 80, .term_h = 24, .tty_raw = false };
    Backend backend;
    Session *current_session = session_create();
    (void)current_session;
    msg = read_message(fd, &last_activity);
    if (!msg) { close(fd); return NULL; }
    schema_error = NULL;
    if (!schema_validate(msg, widget_schema, &schema_error)) { send_error(fd, schema_error); free(schema_error); free(msg); close(fd); return NULL; }
    cJSON *json = cJSON_Parse(msg);
    if (!json) { free(msg); close(fd); return NULL; }
    cJSON *type = cJSON_GetObjectItem(json, "type");
    if (type && type->valuestring && strcmp(type->valuestring, "handshake") == 0) { cJSON_Delete(json); free(msg); msg = read_message(fd, &last_activity); if (!msg) { close(fd); return NULL; } json = cJSON_Parse(msg); }
    WidgetRequest *req = widget_request_parse(msg);
    free(msg);
    if (!req) { send_error(fd, "Invalid widget request"); if (json) cJSON_Delete(json); close(fd); return NULL; }
    if (json) cJSON_Delete(json);
    if (req->session_id) { Session *s = session_find(req->session_id); if (s) current_session = s; }
    socket_mode = req->relay;
    if (req->headless) { headless_backend_init(&hl, 80, 24); backend.vtable = &headless_vtable; backend.data = &hl; }
    else if (socket_mode) {
        if (sb.tty_fd < 0) { const char *tty_path = req->tty ? req->tty : "/dev/tty"; if (!tty_is_owned_by_user(tty_path)) { send_error(fd, "Permission denied for TTY"); widget_request_free(req); close(fd); return NULL; } sb.tty_fd = open(tty_path, O_RDWR); if (sb.tty_fd < 0) { send_error(fd, "Cannot open /dev/tty"); widget_request_free(req); close(fd); return NULL; } struct winsize ws; if (ioctl(sb.tty_fd, TIOCGWINSZ, &ws) == 0) { sb.term_w = ws.ws_col; sb.term_h = ws.ws_row; } }
        backend.vtable = &socket_vtable; backend.data = &sb;
    } else {
        if (!terminal_backend_init(&t, NULL)) { send_error(fd, "No terminal available"); widget_request_free(req); close(fd); return NULL; }
        term_ready = true; backend.vtable = &terminal_vtable; backend.data = &t;
    }
    Widget *w = widget_registry_create(req);
    WidgetResponse resp;
    if (w) { resp = session_run(w, &backend); widget_destroy(w); }
    else { resp.result = NULL; resp.cancelled = true; resp.error = "Unknown widget"; }
    char *response_json = widget_response_to_json(&resp);
    write(fd, response_json, strlen(response_json)); write(fd, "\n", 1);
    free(response_json); widget_request_free(req);
    if (term_ready) terminal_backend_destroy(&t);
    if (req->headless) headless_backend_destroy(&hl);
    if (socket_mode) { if (sb.tty_raw) tcsetattr(sb.tty_fd, TCSAFLUSH, &sb.orig); if (sb.tty_fd >= 0) close(sb.tty_fd); }
    close(fd);
    return NULL;
}

bool daemon_run(const char *socket_path) {
    FillyConfig cfg;
    config_load(&cfg, NULL);
    log_init(cfg.log_path[0] ? cfg.log_path : NULL, cfg.log_level);
    if (cfg.sandbox) set_use_sandbox(true);
    load_plugins();
    InternalClipboard *ic = clipboard_internal_new();
    ClipboardInterface ci = clipboard_internal_interface(ic);
    session_set_clipboard(&ci);
    Session *restored = NULL;
    int restored_count = checkpoint_restore(&restored);
    if (restored_count > 0) { sessions = restored; session_count = restored_count; }
    const char *actual_path = socket_path ? socket_path : cfg.socket_path;
    char default_path[256];
    if (!actual_path) { const char *xdg = getenv("XDG_RUNTIME_DIR"); if (xdg) snprintf(default_path, sizeof(default_path), "%s/filly.sock", xdg); else snprintf(default_path, sizeof(default_path), "/tmp/filly.sock"); actual_path = default_path; }
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return false;
    unlink(actual_path);
    struct sockaddr_un addr = { .sun_family = AF_UNIX };
    strncpy(addr.sun_path, actual_path, sizeof(addr.sun_path) - 1);
    addr.sun_path[sizeof(addr.sun_path) - 1] = '\0';
    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) { close(fd); return false; }
    fchmod(fd, 0600);
    freopen("/dev/null", "r", stdin);
    freopen("/dev/null", "w", stdout);
    freopen("/dev/null", "w", stderr);
    if (listen(fd, 5) < 0) { close(fd); return false; }
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGHUP, &sa, NULL);
    int conn_count_this_second = 0;
    time_t last_second = time(NULL);
    int max_conn = cfg.max_connections_per_sec > 0 ? cfg.max_connections_per_sec : 10;
    while (daemon_running) {
        time_t now = time(NULL);
        if (now != last_second) { conn_count_this_second = 0; last_second = now; }
        if (conn_count_this_second >= max_conn) { usleep(100000); continue; }
        int client = accept(fd, NULL, NULL);
        if (client < 0) continue;
        conn_count_this_second++;
        if (!check_peer_cred(client)) { send_error(client, "Permission denied: UID mismatch"); close(client); continue; }
        pthread_t tid;
        pthread_create(&tid, NULL, handle_client, (void *)(intptr_t)client);
        pthread_detach(tid);
        static int conn_count = 0;
        if (++conn_count % 10 == 0) checkpoint_save(sessions, session_count);
    }
    checkpoint_save(sessions, session_count);
    close(fd);
    unlink(actual_path);
    return true;
}