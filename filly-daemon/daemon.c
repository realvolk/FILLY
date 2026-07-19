#include "../filly-core/backend.h"
#include "../filly-terminal/terminal.h"
extern BackendVTable terminal_vtable;
#include "daemon.h"
#include "checkpoint.h"
#include "verify.h"
#include "../filly-protocol/protocol.h"
#include "../filly-protocol/schema.h"
#include "../filly-terminal/terminal.h"
#include "../filly-terminal/renderer.h"
#include "../filly-core/session.h"
#include "../filly-core/widget.h"
#include "../filly-headless/headless.h"
#include "../filly-script/fil.h"
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

#define INACTIVITY_TIMEOUT 30

static bool insecure_plugins = false;

void set_insecure_plugins(bool val) {
    insecure_plugins = val;
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
    int hl = snprintf(header, sizeof(header),
        "{\"type\":\"draw\",\"len\":%d}\n", len);
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
            ev.ch = (ch && ch->valuestring && strlen(ch->valuestring) > 0)
                 ? ch->valuestring[0] : 0;
        }
    } else if (type && type->valuestring && strcmp(type->valuestring, "size") == 0) {
        cJSON *w = cJSON_GetObjectItem(msg, "w");
        cJSON *h = cJSON_GetObjectItem(msg, "h");
        if (w && h) {
            s->term_w = w->valueint; s->term_h = h->valueint;
            ev.type = EVENT_RESIZE; ev.w = w->valueint; ev.h = h->valueint;
        }
    }
    cJSON_Delete(msg);
    return ev;
}

static bool sock_teardown(void *self) {
    SocketBackend *s = (SocketBackend *)self;
    if (s->tty_raw && s->tty_fd >= 0) {
        tcsetattr(s->tty_fd, TCSAFLUSH, &s->orig);
        s->tty_raw = false;
    }
    return true;
}

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

static bool tty_is_owned_by_user(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return false;
    return st.st_uid == getuid();
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
            if (!insecure_plugins && !verify_plugin_signature(full)) continue;
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

static Session *sessions = NULL;
static int session_count = 0;

static Session *session_find(const char *id) {
    for (int i = 0; i < session_count; i++)
        if (strcmp(sessions[i].id, id) == 0 && sessions[i].active)
            return &sessions[i];
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

static void *handle_client(void *arg) {
    int fd = (intptr_t)arg;
    char buf[524288];
    bool term_ready = false, socket_mode = false;
    TerminalBackend t;
    HeadlessBackend hl;
    SocketBackend sb = { .fd = fd, .tty_fd = -1, .term_w = 80, .term_h = 24, .tty_raw = false };
    Backend backend;
    Session *current_session = NULL;
    bool use_msgpack = false;
    time_t last_activity = time(NULL);

    while (1) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(fd, &fds);
        struct timeval tv = { .tv_sec = 1, .tv_usec = 0 };
        int ret = select(fd + 1, &fds, NULL, NULL, &tv);
        if (ret < 0) break;
        if (ret == 0) {
            if (time(NULL) - last_activity > INACTIVITY_TIMEOUT) {
                char *err = strdup("{\"type\":\"response\",\"result\":null,\"cancelled\":true,\"error\":\"Inactivity timeout\"}\n");
                write(fd, err, strlen(err)); free(err);
                break;
            }
            continue;
        }
        int n = 0;
        while (n < (int)sizeof(buf) - 1) {
            int r = read(fd, buf + n, 1);
            if (r <= 0) goto done;
            if (buf[n] == '\n') { buf[n] = '\0'; n++; break; }
            n++;
        }
        if (n == 0) break;
        last_activity = time(NULL);

        cJSON *msg = cJSON_Parse(buf);
        if (!msg) {
            char *err = strdup("{\"type\":\"response\",\"result\":null,\"cancelled\":true,\"error\":\"Invalid JSON\"}\n");
            write(fd, err, strlen(err)); free(err);
            continue;
        }

        cJSON *type = cJSON_GetObjectItem(msg, "type");
        if (type && type->valuestring) {
            if (strcmp(type->valuestring, "size") == 0) {
                cJSON *w = cJSON_GetObjectItem(msg, "w");
                cJSON *h = cJSON_GetObjectItem(msg, "h");
                if (w && h) { sb.term_w = w->valueint; sb.term_h = h->valueint; }
                cJSON_Delete(msg); continue;
            }
            if (strcmp(type->valuestring, "quit") == 0) {
                cJSON_Delete(msg);
                char *resp = strdup("{\"type\":\"response\",\"result\":null,\"cancelled\":false}\n");
                write(fd, resp, strlen(resp)); free(resp);
                break;
            }
            if (strcmp(type->valuestring, "handshake") == 0) {
                cJSON *enc = cJSON_GetObjectItem(msg, "encoding");
                if (enc && enc->valuestring && strcmp(enc->valuestring, "msgpack") == 0)
                    use_msgpack = true;
                char *resp = strdup(use_msgpack ?
                    "{\"type\":\"handshake\",\"version\":1,\"encoding\":\"msgpack\"}\n" :
                    "{\"type\":\"handshake\",\"version\":1}\n");
                write(fd, resp, strlen(resp)); free(resp);
                cJSON_Delete(msg); continue;
            }
            if (strcmp(type->valuestring, "ping") == 0) {
                char *resp = strdup("{\"type\":\"pong\"}\n");
                write(fd, resp, strlen(resp)); free(resp);
                cJSON_Delete(msg); continue;
            }
            if (strcmp(type->valuestring, "session") == 0) {
                cJSON *action = cJSON_GetObjectItem(msg, "action");
                cJSON *sid = cJSON_GetObjectItem(msg, "id");
                if (action && action->valuestring) {
                    if (strcmp(action->valuestring, "create") == 0) {
                        current_session = session_create();
                        char resp[256];
                        snprintf(resp, sizeof(resp),
                            "{\"type\":\"session\",\"id\":\"%s\",\"action\":\"created\"}\n",
                            current_session->id);
                        write(fd, resp, strlen(resp));
                    } else if (strcmp(action->valuestring, "attach") == 0 && sid && sid->valuestring) {
                        current_session = session_find(sid->valuestring);
                        if (current_session) {
                            char resp[256];
                            snprintf(resp, sizeof(resp),
                                "{\"type\":\"session\",\"id\":\"%s\",\"action\":\"attached\"}\n",
                                current_session->id);
                            write(fd, resp, strlen(resp));
                        }
                    } else if (strcmp(action->valuestring, "destroy") == 0 && current_session) {
                        store_free(current_session->store);
                        current_session->active = false;
                        current_session = NULL;
                        char *resp = strdup("{\"type\":\"session\",\"action\":\"destroyed\"}\n");
                        write(fd, resp, strlen(resp)); free(resp);
                    }
                }
                cJSON_Delete(msg); continue;
            }
            if (strcmp(type->valuestring, "subscribe") == 0 && current_session) {
                cJSON *keys = cJSON_GetObjectItem(msg, "keys");
                if (keys && keys->type == cJSON_Array) {
                    cJSON *key;
                    cJSON_ArrayForEach(key, keys) {
                        if (key->valuestring)
                            store_subscribe(current_session->store, key->valuestring, fd);
                    }
                }
                cJSON_Delete(msg); continue;
            }
        }

        WidgetRequest *req = widget_request_parse(buf);
        if (req && req->session_id) {
            Session *s = session_find(req->session_id);
            if (s) current_session = s;
        }
        cJSON_Delete(msg);

        if (!req) {
            char *err = strdup("{\"type\":\"response\",\"result\":null,\"cancelled\":true,\"error\":\"Invalid JSON\"}\n");
            write(fd, err, strlen(err)); free(err);
            continue;
        }

        socket_mode = req->relay;

        if (req->headless) {
            headless_backend_init(&hl, 80, 24);
            backend.vtable = &headless_vtable;
            backend.data = &hl;
        } else if (socket_mode) {
            if (sb.tty_fd < 0) {
                const char *tty_path = req->tty ? req->tty : "/dev/tty";
                if (!tty_is_owned_by_user(tty_path)) {
                    char *err = strdup("{\"type\":\"response\",\"result\":null,\"cancelled\":true,\"error\":\"Permission denied for TTY\"}\n");
                    write(fd, err, strlen(err)); free(err);
                    widget_request_free(req); continue;
                }
                sb.tty_fd = open(tty_path, O_RDWR);
                if (sb.tty_fd < 0) {
                    char *err = strdup("{\"type\":\"response\",\"result\":null,\"cancelled\":true,\"error\":\"Cannot open /dev/tty\"}\n");
                    write(fd, err, strlen(err)); free(err);
                    widget_request_free(req); continue;
                }
                struct winsize ws;
                if (ioctl(sb.tty_fd, TIOCGWINSZ, &ws) == 0) {
                    sb.term_w = ws.ws_col; sb.term_h = ws.ws_row;
                }
            }
            backend.vtable = &socket_vtable;
            backend.data = &sb;
        } else {
            if (!term_ready) {
                if (!terminal_backend_init(&t, NULL)) {
                    char *err = strdup("{\"type\":\"response\",\"result\":null,\"cancelled\":true,\"error\":\"No terminal\"}\n");
                    write(fd, err, strlen(err)); free(err);
                    widget_request_free(req); continue;
                }
                term_ready = true;
            }
            backend.vtable = &terminal_vtable;
            backend.data = &t;
        }

        Widget *w = widget_registry_create(req);
        WidgetResponse resp;
        if (w) {
            resp = session_run(w, &backend);
            widget_destroy(w);
        } else {
            resp.result = NULL; resp.cancelled = true; resp.error = "Unknown widget";
        }

        char *json = widget_response_to_json(&resp);
        write(fd, json, strlen(json)); write(fd, "\n", 1);
        free(json);
        widget_request_free(req);

        if (req->headless) headless_backend_destroy(&hl);
        if (socket_mode) {
            if (sb.tty_raw) { tcsetattr(sb.tty_fd, TCSAFLUSH, &sb.orig); sb.tty_raw = false; }
            if (sb.tty_fd >= 0) { close(sb.tty_fd); sb.tty_fd = -1; }
            break;
        }
    }
done:
    if (term_ready && !socket_mode) terminal_backend_destroy(&t);
    if (sb.tty_fd >= 0) {
        if (sb.tty_raw) tcsetattr(sb.tty_fd, TCSAFLUSH, &sb.orig);
        close(sb.tty_fd);
    }
    close(fd);
    return NULL;
}

bool daemon_run(const char *socket_path) {
    load_plugins();
    Session *restored = NULL;
    int restored_count = checkpoint_restore(&restored);
    if (restored_count > 0) {
        sessions = restored;
        session_count = restored_count;
    }
    const char *actual_path = socket_path;
    char default_path[256];
    if (!actual_path) {
        const char *xdg = getenv("XDG_RUNTIME_DIR");
        if (xdg) snprintf(default_path, sizeof(default_path), "%s/filly.sock", xdg);
        else snprintf(default_path, sizeof(default_path), "/tmp/filly.sock");
        actual_path = default_path;
    }
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return false;
    unlink(actual_path);
    struct sockaddr_un addr = { .sun_family = AF_UNIX };
    strncpy(addr.sun_path, actual_path, sizeof(addr.sun_path) - 1);
    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) { close(fd); return false; }
    fchmod(fd, 0600);
    freopen("/dev/null", "r", stdin);
    freopen("/dev/null", "w", stdout);
    freopen("/dev/null", "w", stderr);
    if (listen(fd, 5) < 0) { close(fd); return false; }
    while (1) {
        int client = accept(fd, NULL, NULL);
        if (client < 0) continue;
        pthread_t tid;
        pthread_create(&tid, NULL, handle_client, (void *)(intptr_t)client);
        pthread_detach(tid);
        static int conn_count = 0;
        if (++conn_count % 10 == 0) checkpoint_save(sessions, session_count);
    }
    close(fd);
    return true;
}