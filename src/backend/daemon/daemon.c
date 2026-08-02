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
#include "protocol/msgpack.h"
#include "backend/terminal/terminal.h"
#include "backend/terminal/renderer.h"
#include "core/session.h"
#include "core/widget.h"
#include "backend/headless/headless.h"
#include "script/fil.h"
#include "core/clipboard.h"
#include "core/log.h"
#include "core/config.h"
#include "core/shm_ipc.h"
#include "core/theme.h"
#include "filly-port/port.h"
#include "core/i18n.h"
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
#include <sys/mman.h>
#if FILLY_KQUEUE
#include <sys/event.h>
#endif
#ifdef FILLY_GCORE
#include "backend/gcore/backend.h"
#include "backend/gcore/wayland-clipboard.h"
#endif

#define INACTIVITY_TIMEOUT 30
#define MAX_SOCKET_BUFFER (1024 * 1024)

static bool insecure_plugins = false;
bool use_sandbox = false;
static int daemon_socket_fd = -1;
static int shm_fd = -1;
static void *shm_addr = NULL;
static bool use_msgpack = false;

typedef enum {
    PROFILE_SSH,
    PROFILE_LOCAL_TTY,
    PROFILE_WAYLAND,
    PROFILE_X11,
    PROFILE_HEADLESS
} DeviceProfile;

static DeviceProfile detect_device_profile(void) {
    if (getenv("SSH_TTY") || getenv("SSH_CLIENT") || getenv("SSH_CONNECTION"))
        return PROFILE_SSH;
    if (getenv("WAYLAND_DISPLAY"))
        return PROFILE_WAYLAND;
    if (getenv("DISPLAY"))
        return PROFILE_X11;
    if (isatty(STDIN_FILENO) && isatty(STDOUT_FILENO))
        return PROFILE_LOCAL_TTY;
    return PROFILE_HEADLESS;
}

void set_insecure_plugins(bool val) { insecure_plugins = val; }
void set_use_sandbox(bool val) { use_sandbox = val; }

static volatile sig_atomic_t daemon_running = 1;
static void handle_signal(int sig) {
    if (sig == SIGHUP && daemon_socket_fd >= 0) {
        char fd_str[16];
        snprintf(fd_str, sizeof(fd_str), "%d", daemon_socket_fd);
#ifdef __linux__
        const char *argv[] = {"/proc/self/exe", "daemon", "--restore-fd", fd_str, NULL};
        execve("/proc/self/exe", (char *const *)argv, NULL);
#else
        const char *argv[] = {"filly", "daemon", "--restore-fd", fd_str, NULL};
        execvp("filly", (char *const *)argv);
#endif
        _exit(1);
    }
    daemon_running = 0;
}

static bool check_peer_cred(int fd) {
    filly_ucred_t cred;
    if (filly_get_peer_cred(fd, &cred) < 0) return false;
    return cred.uid == getuid();
}

static bool tty_is_owned_by_user(const char *path) {
    return access(path, R_OK | W_OK) == 0;
}

typedef struct {
    int fd; int tty_fd; int term_w, term_h;
    struct termios orig; bool tty_raw;
} SocketBackend;

static bool sock_setup(void *self) {
    SocketBackend *s = (SocketBackend *)self;
    if (s->tty_fd < 0) return false;
    tcgetattr(s->tty_fd, &s->orig);
    struct termios raw = s->orig;
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_iflag &= ~(IXON | ICRNL | BRKINT | INPCK | ISTRIP);
    raw.c_cflag |= CS8; raw.c_oflag &= ~OPOST;
    raw.c_cc[VMIN] = 0; raw.c_cc[VTIME] = 1;
    tcsetattr(s->tty_fd, TCSAFLUSH, &raw);
    s->tty_raw = true;
    return true;
}

static bool sock_draw(void *self, RenderTree *tree) {
    SocketBackend *s = (SocketBackend *)self;
    char buf[524288];
    render_tree_to_buf(tree, 0, 0, s->term_w, s->term_h, buf, sizeof(buf));
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
    char line[256]; int i = 0;
    while (i < (int)sizeof(line)-1) {
        if (read(s->fd, line+i, 1) <= 0) { Event ev = {0}; return ev; }
        if (line[i] == '\n') { line[i] = '\0'; break; }
        i++;
    }
    Event ev = { .type = EVENT_NONE };
    cJSON *msg = cJSON_Parse(line);
    if (!msg) return ev;
    cJSON *type = cJSON_GetObjectItem(msg, "type");
    if (type && type->valuestring) {
        if (!strcmp(type->valuestring, "key")) {
            cJSON *code = cJSON_GetObjectItem(msg, "code");
            cJSON *ch = cJSON_GetObjectItem(msg, "ch");
            if (code) { ev.type = EVENT_KEY; ev.code = (KeyCode)code->valueint;
                ev.ch = (ch && ch->valuestring && ch->valuestring[0]) ? ch->valuestring[0] : 0; }
        } else if (!strcmp(type->valuestring, "size")) {
            cJSON *w = cJSON_GetObjectItem(msg, "w"), *h = cJSON_GetObjectItem(msg, "h");
            if (w && h) { s->term_w = w->valueint; s->term_h = h->valueint;
                ev.type = EVENT_RESIZE; ev.w = w->valueint; ev.h = h->valueint; }
        } else if (!strcmp(type->valuestring, "mouse")) {
            cJSON *x = cJSON_GetObjectItem(msg, "x"), *y = cJSON_GetObjectItem(msg, "y");
            cJSON *btn = cJSON_GetObjectItem(msg, "button"), *st = cJSON_GetObjectItem(msg, "state");
            if (x && y) { ev.type = EVENT_MOUSE_BUTTON; ev.x = x->valueint; ev.y = y->valueint;
                ev.button = btn ? btn->valueint : 0;
                if (st && st->valuestring) {
                    if (!strcmp(st->valuestring, "press")) ev.mouse_state = MOUSE_PRESS;
                    else if (!strcmp(st->valuestring, "release")) ev.mouse_state = MOUSE_RELEASE;
                }
            }
        }
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
    .wait_frame = NULL, .copy_to_clipboard = NULL, .paste_from_clipboard = NULL,
    .is_interactive = true,
};

static void send_error(int fd, const char *msg) {
    char buf[512];
    snprintf(buf, sizeof(buf), "{\"type\":\"response\",\"result\":null,\"cancelled\":true,\"error\":\"%s\"}\n", msg);
    write(fd, buf, strlen(buf));
}

static void send_ok(int fd) {
    write(fd, "{\"type\":\"response\",\"result\":null,\"cancelled\":false}\n", 52);
}

static void send_response_msgpack(int fd, const char *json) {
    if (!use_msgpack) {
        write(fd, json, strlen(json));
        write(fd, "\n", 1);
        return;
    }
    cJSON *root = cJSON_Parse(json);
    if (!root) return;
    uint8_t buf[524288];
    MsgPackWriter w;
    msgpack_writer_init(&w, buf, sizeof(buf));
    cJSON *type = cJSON_GetObjectItem(root, "type");
    cJSON *result = cJSON_GetObjectItem(root, "result");
    cJSON *cancelled = cJSON_GetObjectItem(root, "cancelled");
    cJSON *error = cJSON_GetObjectItem(root, "error");
    msgpack_write_map_header(&w, 4);
    msgpack_write_str(&w, "type", 4);
    msgpack_write_str(&w, type && type->valuestring ? type->valuestring : "", type && type->valuestring ? strlen(type->valuestring) : 0);
    msgpack_write_str(&w, "result", 6);
    if (result) {
        char *rj = cJSON_PrintUnformatted(result);
        msgpack_write_str(&w, rj, strlen(rj));
        free(rj);
    } else {
        msgpack_write_nil(&w);
    }
    msgpack_write_str(&w, "cancelled", 9);
    msgpack_write_bool(&w, cancelled && (cancelled->type == cJSON_True || cancelled->valueint));
    msgpack_write_str(&w, "error", 5);
    if (error && error->valuestring) msgpack_write_str(&w, error->valuestring, strlen(error->valuestring));
    else msgpack_write_nil(&w);
    cJSON_Delete(root);
    write(fd, buf, w.pos);
}

static const char *plugin_dir_path = NULL;
static void **loaded_libs = NULL;
static int loaded_lib_count = 0;

void load_plugins(void) {
    const char *home = getenv("HOME");
    if (!home) return;
    char path[1024];
    snprintf(path, sizeof(path), "%s/.config/filly/plugins", home);
    plugin_dir_path = strdup(path);
    DIR *d = opendir(path);
    if (!d) return;
    struct dirent *entry;
    while ((entry = readdir(d))) {
        int len = strlen(entry->d_name);
        if (len > 3 && !strcmp(entry->d_name + len - 3, ".so")) {
            char full[2048];
            snprintf(full, sizeof(full), "%s/%s", path, entry->d_name);
            if (!insecure_plugins && !verify_plugin_signature(full)) continue;
            if (use_sandbox) {
                SandboxHandle sh;
                if (sandbox_spawn(full, NULL, NULL, &sh)) { sandbox_get_result(&sh); sandbox_cleanup(&sh); }
                continue;
            }
            void *lib = dlopen(full, RTLD_NOW | RTLD_GLOBAL);
            if (lib) {
                void (*reg)(void (*)(const char *, WidgetFactory));
                *(void **)(&reg) = dlsym(lib, "register_plugins");
                if (reg) {
                    reg(widget_registry_register_plugin);
                    loaded_libs = realloc(loaded_libs, (loaded_lib_count+1)*sizeof(void*));
                    loaded_libs[loaded_lib_count++] = lib;
                } else LOG_WARN("Plugin %s has no register_plugins", full);
            } else LOG_ERROR("dlopen failed: %s", dlerror());
        }
    }
    closedir(d);
    if (g_active_theme) theme_add_plugin_overrides(g_active_theme, path);
}

static void unload_plugins(void) {
    widget_registry_clear_plugins();
    for (int i = 0; i < loaded_lib_count; i++) if (loaded_libs[i]) dlclose(loaded_libs[i]);
    free(loaded_libs); loaded_libs = NULL; loaded_lib_count = 0;
}

static void reload_plugins(void) { unload_plugins(); load_plugins(); }

static Session *sessions = NULL;
static int session_count = 0;

static Session *session_find(const char *id) {
    for (int i = 0; i < session_count; i++)
        if (!strcmp(sessions[i].id, id) && sessions[i].active) return &sessions[i];
    return NULL;
}

static Session *session_create(void) {
    sessions = realloc(sessions, (session_count+1)*sizeof(Session));
    Session *s = &sessions[session_count++];
    snprintf(s->id, sizeof(s->id), "%lx%lx", (unsigned long)time(NULL), (unsigned long)pthread_self());
    s->store = store_new(); s->active = true;
    return s;
}

static char *read_message(int fd, time_t *last_activity) {
    char *buf = malloc(524288); int n = 0;
    while (n < 524287) {
        fd_set fds; FD_ZERO(&fds); FD_SET(fd, &fds);
        struct timeval tv = {1, 0};
        int ret = select(fd+1, &fds, NULL, NULL, &tv);
        if (ret < 0) { free(buf); return NULL; }
        if (ret == 0) {
            if (time(NULL) - *last_activity > INACTIVITY_TIMEOUT) { free(buf); return NULL; }
            continue;
        }
        int r = read(fd, buf+n, 1);
        if (r <= 0) { free(buf); return NULL; }
        if (buf[n] == '\n') { buf[n] = '\0'; *last_activity = time(NULL); return buf; }
        n++;
    }
    free(buf); return NULL;
}

static const char *widget_schema =
    "{\"type\":\"object\",\"properties\":{\"widget\":{\"type\":\"string\"},\"params\":{\"type\":\"object\"}}}";

static void dispatch_message(int fd, const char *msg, Session *session, Backend *backend, bool *running) {
    char *schema_error = NULL;
    if (!schema_validate(msg, widget_schema, &schema_error)) { send_error(fd, schema_error); free(schema_error); return; }
    cJSON *json = cJSON_Parse(msg);
    if (!json) return;
    cJSON *type = cJSON_GetObjectItem(json, "type");
    if (type && type->valuestring) {
        if (!strcmp(type->valuestring, "quit")) { cJSON_Delete(json); send_ok(fd); *running = false; return; }
        if (!strcmp(type->valuestring, "ping")) { write(fd, "{\"type\":\"pong\"}\n", 16); cJSON_Delete(json); return; }
        if (!strcmp(type->valuestring, "subscribe")) {
            cJSON *keys = cJSON_GetObjectItem(json, "keys");
            if (keys && keys->type == cJSON_Array) {
                cJSON *key; cJSON_ArrayForEach(key, keys)
                    if (key->valuestring) store_subscribe(session->store, key->valuestring, fd);
            }
            cJSON_Delete(json); return;
        }
        if (!strcmp(type->valuestring, "unsubscribe")) {
            cJSON *keys = cJSON_GetObjectItem(json, "keys");
            if (keys && keys->type == cJSON_Array) {
                cJSON *key; cJSON_ArrayForEach(key, keys)
                    if (key->valuestring) store_unsubscribe(session->store, key->valuestring, fd);
            }
            send_ok(fd); cJSON_Delete(json); return;
        }
        if (!strcmp(type->valuestring, "reload_theme")) {
            if (g_active_theme) theme_free(g_active_theme);
            g_active_theme = theme_load("themes/forge.json");
            if (!g_active_theme) g_active_theme = theme_load_directory("themes");
            if (!g_active_theme) g_active_theme = theme_default();
            theme_merge_user_override(g_active_theme);
            if (plugin_dir_path) theme_add_plugin_overrides(g_active_theme, plugin_dir_path);
            send_ok(fd); cJSON_Delete(json); return;
        }
        if (!strcmp(type->valuestring, "reload_plugins")) { reload_plugins(); send_ok(fd); cJSON_Delete(json); return; }
        if (!strcmp(type->valuestring, "set_accessibility")) {
            cJSON *profile = cJSON_GetObjectItem(json, "profile");
            if (profile && profile->valuestring && g_active_theme)
                theme_merge_accessibility_profile(g_active_theme, profile->valuestring);
            send_ok(fd); cJSON_Delete(json); return;
        }
        if (!strcmp(type->valuestring, "session")) {
            cJSON *action = cJSON_GetObjectItem(json, "action");
            if (action && action->valuestring) {
                if (!strcmp(action->valuestring, "create")) {
                    Session *s = session_create();
                    dprintf(fd, "{\"type\":\"response\",\"result\":\"%s\",\"cancelled\":false}\n", s->id);
                } else if (!strcmp(action->valuestring, "destroy")) send_ok(fd);
            }
            cJSON_Delete(json); return;
        }
    }
    WidgetRequest *req = widget_request_parse(msg);
    cJSON_Delete(json);
    if (!req) { send_error(fd, "Invalid JSON"); return; }
    Widget *w = widget_registry_create(req);
    WidgetResponse resp;
    if (w) { resp = session_run(w, backend); widget_destroy(w); }
    else { resp.result = NULL; resp.cancelled = true; resp.error = "Unknown widget"; }
    char *out = widget_response_to_json(&resp);
    send_response_msgpack(fd, out);
    free(out); widget_request_free(req);
}

#ifdef FILLY_GCORE
static void *handle_gui_client(void *arg) {
    int fd = (intptr_t)arg;
    GCoreBackend *gcore = calloc(1, sizeof(GCoreBackend));
    if (!gcore_backend_init(gcore, GCORE_DRM, NULL)) { send_error(fd, "GUI backend unavailable"); close(fd); free(gcore); return NULL; }

    Backend *backends[3]; int nb = 0;
    backends[nb++] = &(Backend){ .vtable = &gcore_vtable, .data = gcore };

    TerminalBackend *t = NULL;
    if (terminal_backend_init(t = calloc(1, sizeof(TerminalBackend)), NULL))
        backends[nb++] = &(Backend){ .vtable = &terminal_vtable, .data = t };

    Session *session = session_create();
    for (int i = 0; i < nb; i++) backends[i]->vtable->setup(backends[i]->data);

    time_t last_activity = time(NULL);
    bool running = true;
    while (running) {
        char *msg = read_message(fd, &last_activity);
        if (!msg) break;
        dispatch_message(fd, msg, session, backends[0], &running);
        free(msg);
    }

    for (int i = 0; i < nb; i++) backends[i]->vtable->teardown(backends[i]->data);
    gcore_backend_destroy(gcore); free(gcore);
    if (t) { terminal_backend_destroy(t); free(t); }
    close(fd);
    return NULL;
}
#endif

static void *handle_client(void *arg) {
    int fd = (intptr_t)arg;
    time_t last_activity = time(NULL);
    char *msg = read_message(fd, &last_activity);
    if (!msg) { close(fd); return NULL; }

    cJSON *first = cJSON_Parse(msg);
    bool gui_req = false;
    bool is_handshake = false;
    use_msgpack = false;
    if (first) {
        cJSON *gui = cJSON_GetObjectItem(first, "gui");
        if (gui) gui_req = gui->valueint;
        cJSON *type = cJSON_GetObjectItem(first, "type");
        if (type && type->valuestring && !strcmp(type->valuestring, "handshake")) {
            is_handshake = true;
            cJSON *enc = cJSON_GetObjectItem(first, "encoding");
            if (enc && enc->valuestring && !strcmp(enc->valuestring, "msgpack"))
                use_msgpack = true;
        }
        cJSON_Delete(first);
    }

#ifdef FILLY_GCORE
    if (gui_req) {
        free(msg);
        handle_gui_client(arg);
        return NULL;
    }
#endif

    char *widget_msg = msg;
    if (is_handshake) {
        free(msg);
        widget_msg = read_message(fd, &last_activity);
        if (!widget_msg) { close(fd); return NULL; }
    }

    cJSON *json = cJSON_Parse(widget_msg);
    if (!json) { free(widget_msg); close(fd); return NULL; }

    cJSON *type_check = cJSON_GetObjectItem(json, "type");
    cJSON *widget_check = cJSON_GetObjectItem(json, "widget");
    
    if (type_check && type_check->valuestring && !widget_check) {
        /* Non-widget control message (reload_theme, ping, subscribe, etc.) */
        Session *session = session_create();
        bool running = true;
        HeadlessBackend hl;
        headless_backend_init(&hl, 80, 24);
        Backend backend = { .vtable = &headless_vtable, .data = &hl };
        dispatch_message(fd, widget_msg, session, &backend, &running);
        headless_backend_destroy(&hl);
        cJSON_Delete(json);
        free(widget_msg);
        close(fd);
        return NULL;
    }

    WidgetRequest *req = widget_request_parse(widget_msg);
    if (!req) { cJSON_Delete(json); free(widget_msg); send_error(fd, "Invalid widget request"); close(fd); return NULL; }

    Session *session = session_create();
    if (req->session_id) { Session *s = session_find(req->session_id); if (s) session = s; }
    (void)session;

    HeadlessBackend hl;
    SocketBackend sb = { .fd = fd, .tty_fd = -1, .term_w = 80, .term_h = 24 };
    Backend backend;

    if (req->relay) {
        if (sb.tty_fd < 0) {
            const char *tty_path = req->tty ? req->tty : "/dev/tty";
            if (!tty_is_owned_by_user(tty_path)) { widget_request_free(req); cJSON_Delete(json); free(widget_msg); send_error(fd, "Permission denied"); close(fd); return NULL; }
            sb.tty_fd = open(tty_path, O_RDWR);
            if (sb.tty_fd < 0) { widget_request_free(req); cJSON_Delete(json); free(widget_msg); send_error(fd, "Cannot open /dev/tty"); close(fd); return NULL; }
            struct winsize ws;
            if (ioctl(sb.tty_fd, TIOCGWINSZ, &ws) == 0) { sb.term_w = ws.ws_col; sb.term_h = ws.ws_row; }
        }
        backend.vtable = &socket_vtable; backend.data = &sb;
    } else {
        headless_backend_init(&hl, 80, 24);
        backend.vtable = &headless_vtable;
        backend.data = &hl;
    }

    Widget *w = widget_registry_create(req);
    WidgetResponse resp;
    if (w) { resp = session_run(w, &backend); widget_destroy(w); }
    else { resp.result = NULL; resp.cancelled = true; resp.error = "Unknown widget"; }
    char *out = widget_response_to_json(&resp);
    send_response_msgpack(fd, out);
    free(out); widget_request_free(req);
    if (!req->relay) headless_backend_destroy(&hl);
    else { if (sb.tty_raw) tcsetattr(sb.tty_fd, TCSAFLUSH, &sb.orig); if (sb.tty_fd >= 0) close(sb.tty_fd); }
    cJSON_Delete(json);
    free(widget_msg);
    close(fd);
    return NULL;
}

bool daemon_run(const char *socket_path, const char *theme_path) {
    FillyConfig cfg; config_load(&cfg, NULL);
    log_init(cfg.log_path[0] ? cfg.log_path : NULL, cfg.log_level);
    i18n_init();
    if (cfg.sandbox) set_use_sandbox(true);

    DeviceProfile profile = detect_device_profile();
    LOG_INFO("daemon: device profile %d", profile);

    g_active_theme = theme_load("themes/forge.json");
    if (!g_active_theme) g_active_theme = theme_load_directory("themes");
    if (!g_active_theme) g_active_theme = theme_default();
    if (theme_path) theme_merge_app_override(g_active_theme, theme_path);
    theme_merge_user_override(g_active_theme);

    if (profile == PROFILE_SSH && g_active_theme) {
        theme_merge_accessibility_profile(g_active_theme, "high-contrast");
    }

    load_plugins();
    InternalClipboard *ic = clipboard_internal_new();
    ClipboardInterface ci = clipboard_internal_interface(ic);
    session_set_clipboard(&ci);
    Session *restored = NULL;
    int restored_count = checkpoint_restore(&restored);
    if (restored_count > 0) { sessions = restored; session_count = restored_count; }

    shm_fd = shm_ipc_create();
    if (shm_fd >= 0) { shm_addr = shm_ipc_map(shm_fd); }

    const char *actual_path = socket_path ? socket_path : cfg.socket_path;
    char default_path[256];
    if (!actual_path) {
        const char *xdg = getenv("XDG_RUNTIME_DIR");
        snprintf(default_path, sizeof(default_path), "%s/filly.sock", xdg ? xdg : "/tmp");
        actual_path = default_path;
    }

    daemon_socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (daemon_socket_fd < 0) return false;
    unlink(actual_path);
    struct sockaddr_un addr = { .sun_family = AF_UNIX };
    size_t path_len = strlen(actual_path);
    if (path_len >= sizeof(addr.sun_path)) path_len = sizeof(addr.sun_path) - 1;
    memcpy(addr.sun_path, actual_path, path_len);
    addr.sun_path[path_len] = '\0';
    if (bind(daemon_socket_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) { close(daemon_socket_fd); return false; }
    fchmod(daemon_socket_fd, 0600);
    freopen("/dev/null", "r", stdin);
    freopen("/dev/null", "w", stdout);
    freopen("/dev/null", "w", stderr);
    if (listen(daemon_socket_fd, 5) < 0) { close(daemon_socket_fd); return false; }

#if FILLY_PLEDGE
    pledge("stdio unix proc", NULL);
#endif
#if FILLY_CAPSICUM
    cap_enter();
#endif

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGHUP, &sa, NULL);

#if FILLY_INOTIFY
    int inotify_fd = -1, inotify_wd = -1;
    const char *home = getenv("HOME");
    if (home) {
        inotify_fd = inotify_init1(IN_NONBLOCK);
        if (inotify_fd >= 0) {
            inotify_wd = inotify_add_watch(inotify_fd, "themes", IN_CLOSE_WRITE | IN_CREATE);
            if (inotify_wd < 0) {
                char user_themes[1024];
                snprintf(user_themes, sizeof(user_themes), "%s/.config/filly/styles", home);
                inotify_wd = inotify_add_watch(inotify_fd, user_themes, IN_CLOSE_WRITE | IN_CREATE);
            }
        }
    }
#elif FILLY_KQUEUE
    int kq = -1;
    struct kevent ev[2];
    const char *home = getenv("HOME");
    if (home) {
        kq = kqueue();
        if (kq >= 0) {
            int themes_fd = open("themes", O_RDONLY);
            if (themes_fd >= 0) {
                EV_SET(&ev[0], themes_fd, EVFILT_VNODE, EV_ADD | EV_ENABLE | EV_CLEAR,
                       NOTE_WRITE | NOTE_EXTEND, 0, 0);
                kevent(kq, &ev[0], 1, NULL, 0, NULL);
            }
            char user_themes[1024];
            snprintf(user_themes, sizeof(user_themes), "%s/.config/filly/styles", home);
            int user_fd = open(user_themes, O_RDONLY);
            if (user_fd >= 0) {
                EV_SET(&ev[1], user_fd, EVFILT_VNODE, EV_ADD | EV_ENABLE | EV_CLEAR,
                       NOTE_WRITE | NOTE_EXTEND, 0, 0);
                kevent(kq, &ev[1], 1, NULL, 0, NULL);
            }
        }
    }
#endif

    int conn_count_this_second = 0;
    time_t last_second = time(NULL);
    int max_conn = cfg.max_connections_per_sec > 0 ? cfg.max_connections_per_sec : 10;

    while (daemon_running) {
        time_t now = time(NULL);
        if (now != last_second) { conn_count_this_second = 0; last_second = now; }
        if (conn_count_this_second >= max_conn) {
            struct timespec ts = {0, 100000000};
            nanosleep(&ts, NULL);
            continue;
        }

#if FILLY_INOTIFY
        if (inotify_fd >= 0) {
            struct inotify_event iev;
            if (read(inotify_fd, &iev, sizeof(iev)) > 0) {
                if (g_active_theme) theme_free(g_active_theme);
                g_active_theme = theme_load("themes/forge.json");
                if (!g_active_theme) g_active_theme = theme_load_directory("themes");
                if (!g_active_theme) g_active_theme = theme_default();
                if (theme_path) theme_merge_app_override(g_active_theme, theme_path);
                theme_merge_user_override(g_active_theme);
                if (plugin_dir_path) theme_add_plugin_overrides(g_active_theme, plugin_dir_path);
            }
        }
#elif FILLY_KQUEUE
        if (kq >= 0) {
            struct kevent kev;
            struct timespec kts = {0, 0};
            if (kevent(kq, NULL, 0, &kev, 1, &kts) > 0) {
                if (g_active_theme) theme_free(g_active_theme);
                g_active_theme = theme_load("themes/forge.json");
                if (!g_active_theme) g_active_theme = theme_load_directory("themes");
                if (!g_active_theme) g_active_theme = theme_default();
                if (theme_path) theme_merge_app_override(g_active_theme, theme_path);
                theme_merge_user_override(g_active_theme);
                if (plugin_dir_path) theme_add_plugin_overrides(g_active_theme, plugin_dir_path);
            }
        }
#endif

        int client = accept(daemon_socket_fd, NULL, NULL);
        if (client < 0) continue;
        conn_count_this_second++;
        if (!check_peer_cred(client)) { send_error(client, "Permission denied: UID mismatch"); close(client); continue; }
        pthread_t tid;
        pthread_create(&tid, NULL, handle_client, (void*)(intptr_t)client);
        pthread_detach(tid);
        static int conn_count = 0;
        if (++conn_count % 10 == 0) checkpoint_save(sessions, session_count);
    }

    checkpoint_save(sessions, session_count);
    close(daemon_socket_fd);
    unlink(actual_path);
    if (shm_addr) shm_ipc_unmap(shm_addr);
    if (shm_fd >= 0) { shm_unlink(SHM_NAME); close(shm_fd); }

#if FILLY_INOTIFY
    if (inotify_wd >= 0) inotify_rm_watch(inotify_fd, inotify_wd);
    if (inotify_fd >= 0) close(inotify_fd);
#elif FILLY_KQUEUE
    if (kq >= 0) close(kq);
#endif

    return true;
}