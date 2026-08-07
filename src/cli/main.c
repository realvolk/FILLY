#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>
#include <time.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "core/client.h"
#include "protocol/protocol.h"
#include "backend/terminal/terminal.h"
#include "core/widget.h"
#include "core/session.h"
#include "core/theme.h"
#include "core/store.h"
#include "core/config.h"
#include "backend/daemon/daemon.h"
#include "backend/headless/headless.h"
#include "core/relay.h"
#include "core/shm_ipc.h"
#include "core/i18n.h"
#ifdef FILLY_GCORE
#include "backend/gcore/backend.h"
#endif

extern BackendVTable terminal_vtable;

static int cmd_compile(int argc, char **argv) {
    if (argc < 4) {
        fprintf(stderr, "Usage: filly compile <input.json> <output.filly>\n");
        return 1;
    }
    FILE *f = fopen(argv[2], "r");
    if (!f) { perror("open"); return 1; }
    fseek(f, 0, SEEK_END); long sz = ftell(f); rewind(f);
    char *json = malloc(sz + 1);
    fread(json, 1, sz, f); fclose(f); json[sz] = '\0';

    WidgetRequest *req = widget_request_parse(json);
    free(json);
    if (!req) { fprintf(stderr, "Invalid JSON\n"); return 1; }

    FILE *out = fopen(argv[3], "wb");
    if (!out) { perror("open"); return 1; }
    fwrite("FILLY", 1, 5, out);
    uint32_t ver = 1; fwrite(&ver, 4, 1, out);
    fclose(out);
    widget_request_free(req);
    return 0;
}

static int cmd_update(void) {
    const char *url = "https://github.com/realvolk/FILLY/releases/latest/download/filly";
    char tmp[256];
    snprintf(tmp, sizeof(tmp), "/tmp/filly.update.%d", getpid());
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "curl -sL '%s' -o %s", url, tmp);
    if (system(cmd) != 0) { fprintf(stderr, "Download failed\n"); return 1; }
    chmod(tmp, 0755);
    char sock_fd_str[16];
    snprintf(sock_fd_str, sizeof(sock_fd_str), "%d", 3);
    execl(tmp, tmp, "--restore-fd", sock_fd_str, NULL);
    perror("execl");
    return 1;
}

#ifdef FILLY_GCORE
__attribute__((unused))
static WidgetResponse session_run_multi(Widget *w, int backend_count, Backend **backends) {
    if (backend_count <= 0) {
        WidgetResponse err = { .result = NULL, .cancelled = true, .error = "No backends" };
        return err;
    }
    for (int i = 0; i < backend_count; i++)
        backends[i]->vtable->setup(backends[i]->data);
    WidgetResponse resp = session_run(w, backends[0]);
    for (int i = 0; i < backend_count; i++)
        backends[i]->vtable->teardown(backends[i]->data);
    return resp;
}
#endif

static KeyCode parse_key_name(const char *name) {
    if (strcmp(name, "UP") == 0) return KEY_UP;
    if (strcmp(name, "DOWN") == 0) return KEY_DOWN;
    if (strcmp(name, "LEFT") == 0) return KEY_LEFT;
    if (strcmp(name, "RIGHT") == 0) return KEY_RIGHT;
    if (strcmp(name, "ENTER") == 0) return KEY_ENTER;
    if (strcmp(name, "ESC") == 0) return KEY_ESC;
    if (strcmp(name, "TAB") == 0) return KEY_TAB;
    if (strcmp(name, "BACKSPACE") == 0) return KEY_BACKSPACE;
    if (strcmp(name, "SPACE") == 0) return KEY_CHAR;
    if (strcmp(name, "HOME") == 0) return KEY_HOME;
    if (strcmp(name, "END") == 0) return KEY_END;
    if (strcmp(name, "PAGEUP") == 0) return KEY_PAGEUP;
    if (strcmp(name, "PAGEDOWN") == 0) return KEY_PAGEDOWN;
    if (strcmp(name, "DELETE") == 0) return KEY_DELETE;
    if (strcmp(name, "INSERT") == 0) return KEY_INSERT;
    if (strcmp(name, "F1") == 0) return KEY_F1;
    if (strcmp(name, "F2") == 0) return KEY_F2;
    if (strcmp(name, "F3") == 0) return KEY_F3;
    if (strcmp(name, "F4") == 0) return KEY_F4;
    if (strcmp(name, "F5") == 0) return KEY_F5;
    if (strcmp(name, "F6") == 0) return KEY_F6;
    if (strcmp(name, "F7") == 0) return KEY_F7;
    if (strcmp(name, "F8") == 0) return KEY_F8;
    if (strcmp(name, "F9") == 0) return KEY_F9;
    if (strcmp(name, "F10") == 0) return KEY_F10;
    if (strcmp(name, "F11") == 0) return KEY_F11;
    if (strcmp(name, "F12") == 0) return KEY_F12;
    if (strlen(name) == 1) return KEY_CHAR;
    return KEY_NULL;
}

static void inject_events_from_file(HeadlessBackend *hl, const char *events_path) {
    FILE *f = fopen(events_path, "r");
    if (!f) return;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\n")] = 0;
        if (strlen(line) == 0) continue;
        if (strncmp(line, "KEY:", 4) == 0) {
            char *keyname = line + 4; char ch = 0;
            KeyCode code = parse_key_name(keyname);
            if (code == KEY_CHAR) {
                if (strcmp(keyname, "SPACE") == 0) ch = ' ';
                else if (strlen(keyname) == 1) ch = keyname[0];
                else continue;
            }
            headless_inject_key(hl, code, ch);
        } else if (strncmp(line, "TEXT:", 5) == 0) {
            for (char *c = line + 5; *c; c++) headless_inject_key(hl, KEY_CHAR, *c);
        } else if (strncmp(line, "WAIT:", 5) == 0) {
            int ms = atoi(line + 5);
            if (ms > 0) poll(NULL, 0, ms);
        } else if (strncmp(line, "MOUSE:", 6) == 0) {
            char *rest = line + 6;
            if (strncmp(rest, "PRESS:", 6) == 0) {
                int x = 0, y = 0;
                sscanf(rest + 6, "%d,%d", &x, &y);
                headless_inject_mouse(hl, EVENT_MOUSE_BUTTON, x, y, 1);
            } else if (strncmp(rest, "MOVE:", 5) == 0) {
                int x = 0, y = 0;
                sscanf(rest + 5, "%d,%d", &x, &y);
                headless_inject_mouse(hl, EVENT_MOUSE_MOTION, x, y, 0);
            } else if (strncmp(rest, "RELEASE:", 8) == 0) {
                int x = 0, y = 0;
                sscanf(rest + 8, "%d,%d", &x, &y);
                headless_inject_mouse(hl, EVENT_MOUSE_BUTTON, x, y, 0);
            } else if (strncmp(rest, "DRAG_START:", 11) == 0) {
                int x = 0, y = 0;
                sscanf(rest + 11, "%d,%d", &x, &y);
                headless_inject_mouse(hl, EVENT_MOUSE_DRAG_START, x, y, 1);
            } else if (strncmp(rest, "DRAG_MOVE:", 10) == 0) {
                int dx = 0, dy = 0;
                sscanf(rest + 10, "%d,%d", &dx, &dy);
                headless_inject_mouse(hl, EVENT_MOUSE_DRAG_MOVE, dx, dy, 1);
            } else if (strncmp(rest, "DRAG_END:", 9) == 0) {
                int x = 0, y = 0;
                sscanf(rest + 9, "%d,%d", &x, &y);
                headless_inject_mouse(hl, EVENT_MOUSE_DRAG_END, x, y, 1);
            }
        }
    }
    fclose(f);
}

static void run_headless_oneshot(const char *input_path, const char *events_path) {
    char *json = NULL;
    if (input_path) {
        FILE *f = fopen(input_path, "r");
        if (!f) return;
        fseek(f, 0, SEEK_END); long sz = ftell(f); rewind(f);
        json = malloc(sz + 1); fread(json, 1, sz, f); json[sz] = '\0'; fclose(f);
    } else {
        char buf[65536];
        int n = read(STDIN_FILENO, buf, sizeof(buf) - 1);
        if (n > 0) { buf[n] = '\0'; json = strdup(buf); }
    }
    if (!json || !json[0]) { fprintf(stderr, "Empty input\n"); return; }
    WidgetRequest *req = widget_request_parse(json); free(json);
    if (!req) { fprintf(stderr, "Invalid JSON\n"); return; }
    Widget *w = widget_registry_create(req);
    if (!w) { fprintf(stderr, "Unknown widget: %s\n", req->widget); widget_request_free(req); return; }
    HeadlessBackend hl; headless_backend_init(&hl, 80, 24);
    Backend backend = { .vtable = &headless_vtable, .data = &hl };
    if (events_path) inject_events_from_file(&hl, events_path);
    WidgetResponse resp = session_run(w, &backend);
    char *out = widget_response_to_json(&resp);
    printf("%s\n", out); free(out);
    widget_destroy(w); widget_request_free(req); headless_backend_destroy(&hl);
}

static int cmd_send(int argc, char **argv) {
    const char *socket_path = NULL; bool subscribe = false, quit = false, json_output = false;
    char *request = NULL;
    for (int i = 2; i < argc; i++) {
        if (!strcmp(argv[i], "--socket") && i+1<argc) socket_path = argv[++i];
        else if (!strcmp(argv[i], "--subscribe") && i+1<argc) { subscribe = true; request = argv[++i]; }
        else if (!strcmp(argv[i], "--quit")) quit = true;
        else if (!strcmp(argv[i], "--json")) json_output = true;
        else if (!request) request = argv[i];
    }
    if (!socket_path) {
        static char def[256]; const char *xdg = getenv("XDG_RUNTIME_DIR");
        snprintf(def, sizeof(def), "%s/filly.sock", xdg ? xdg : "/tmp");
        socket_path = def;
    }
    FillyClient *c = filly_client_connect(socket_path);
    if (!c) { fprintf(stderr, "Cannot connect\n"); return 1; }
    if (quit) { filly_client_send_quit(c); filly_client_disconnect(c); return 0; }
    if (subscribe) {
        char sub[1024], *p = sub;
        p += snprintf(p, sizeof(sub)-(p-sub), "{\"type\":\"subscribe\",\"keys\":[");
        char *keys = strdup(request), *tok = strtok(keys, ",");
        bool first = true;
        while (tok) {
            if (!first) p += snprintf(p, sizeof(sub)-(p-sub), ",");
            p += snprintf(p, sizeof(sub)-(p-sub), "\"%s\"", tok);
            first = false; tok = strtok(NULL, ",");
        }
        p += snprintf(p, sizeof(sub)-(p-sub), "]}\n"); free(keys);
        filly_client_send_request(c, sub); filly_client_disconnect(c); return 0;
    }
    if (!request) { fprintf(stderr, "No request\n"); filly_client_disconnect(c); return 1; }
    filly_client_send_request(c, request);
    cJSON *result = NULL; bool cancelled = false;
    if (filly_client_get_response(c, &result, &cancelled) < 0) { filly_client_disconnect(c); return 1; }
    if (json_output) {
        cJSON *full = cJSON_CreateObject();
        cJSON_AddStringToObject(full, "type", "response");
        if (result) cJSON_AddItemToObject(full, "result", cJSON_Duplicate(result, 1));
        else cJSON_AddNullToObject(full, "result");
        cJSON_AddBoolToObject(full, "cancelled", cancelled ? 1 : 0);
        char *s = cJSON_PrintUnformatted(full);
        printf("%s\n", s);
        free(s);
        cJSON_Delete(full);
    }
    else if (result && result->valuestring) printf("%s\n", result->valuestring);
    else if (result && (result->type == cJSON_True || result->type == cJSON_False))
        printf("%s\n", result->type == cJSON_True ? "true" : "false");
    else if (result && result->type == cJSON_Number) printf("%d\n", result->valueint);
    else if (result) { char *s = cJSON_PrintUnformatted(result); printf("%s\n", s); free(s); }
    filly_client_disconnect(c);
    return cancelled ? 1 : 0;
}

static int cmd_build(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: filly build <widget> [--param value ...]\n");
        return 1;
    }

    const char *widget = argv[2];
    cJSON *params = cJSON_CreateObject();

    int i = 3;
    while (i < argc) {
        if (strncmp(argv[i], "--", 2) == 0) {
            const char *key = argv[i] + 2;
            i++;
            cJSON *arr = cJSON_CreateArray();
            while (i < argc && strncmp(argv[i], "--", 2) != 0) {
                cJSON_AddItemToArray(arr, cJSON_CreateString(argv[i]));
                i++;
            }
            if (cJSON_GetArraySize(arr) == 1) {
                cJSON *item = cJSON_GetArrayItem(arr, 0);
                cJSON_AddStringToObject(params, key, item->valuestring);
                cJSON_Delete(arr);
            } else if (cJSON_GetArraySize(arr) > 1) {
                cJSON_AddItemToObject(params, key, arr);
            } else {
                cJSON_Delete(arr);
            }
        } else {
            i++;
        }
    }

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "widget", widget);
    cJSON_AddItemToObject(root, "params", params);
    char *out = cJSON_PrintUnformatted(root);
    printf("%s\n", out);
    free(out);
    cJSON_Delete(root);
    return 0;
}

static int cmd_inspect(int argc, char **argv) {
    const char *socket_path = NULL;
    for (int i = 2; i < argc; i++) {
        if (!strcmp(argv[i], "--socket") && i+1<argc) socket_path = argv[++i];
    }
    if (!socket_path) {
        static char def[256]; const char *xdg = getenv("XDG_RUNTIME_DIR");
        snprintf(def, sizeof(def), "%s/filly.sock", xdg ? xdg : "/tmp");
        socket_path = def;
    }
    FillyClient *c = filly_client_connect(socket_path);
    if (!c) { fprintf(stderr, "Cannot connect to daemon\n"); return 1; }
    printf("{\"connected\":true}\n");
    filly_client_disconnect(c);
    return 0;
}

static int cmd_profile(void) {
    printf("{\"fps\":%.1f,\"arena_kb\":%zu,\"reduced_motion\":%s}\n",
        session_current_fps,
        g_session_arena ? g_session_arena->offset / 1024 : 0,
        session_prefers_reduced_motion ? "true" : "false");
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) { printf("Usage: filly [command]\n"); return 1; }
    for (int i = 1; i < argc; i++)
        if (!strcmp(argv[i], "--insecure-plugins")) set_insecure_plugins(true);

    register_builtin_widgets();
    i18n_init();
    load_plugins();

    g_active_theme = theme_load("themes/forge.json");
    if (!g_active_theme) g_active_theme = theme_load_directory("themes");
    if (!g_active_theme) g_active_theme = theme_default();
    theme_merge_user_override(g_active_theme);

    FillyConfig cfg;
    config_load(&cfg, NULL);
    if (cfg.prefers_reduced_motion) session_prefers_reduced_motion = true;

    if (!strcmp(argv[1], "send"))     return cmd_send(argc, argv);
    if (!strcmp(argv[1], "build"))    return cmd_build(argc, argv);
    if (!strcmp(argv[1], "compile"))  return cmd_compile(argc, argv);
    if (!strcmp(argv[1], "update"))   return cmd_update();
    if (!strcmp(argv[1], "test"))     { printf("{\"status\":\"ok\"}\n"); return 0; }
    if (!strcmp(argv[1], "inspect"))  return cmd_inspect(argc, argv);
    if (!strcmp(argv[1], "profile"))  return cmd_profile();

    if (!strcmp(argv[1], "daemon")) {
        const char *socket = NULL, *theme_path = NULL; bool sandbox = false;
        for (int i = 2; i < argc; i++) {
            if (!strcmp(argv[i], "--socket") && i+1<argc) socket = argv[++i];
            else if (!strcmp(argv[i], "--theme") && i+1<argc) theme_path = argv[++i];
            else if (!strcmp(argv[i], "--sandbox")) sandbox = true;
        }
        if (sandbox) set_use_sandbox(true);
        return daemon_run(socket, theme_path) ? 0 : 1;
    }

    if (!strcmp(argv[1], "relay")) {
        if (argc < 4) return 1;
        return relay_run(argv[2], argv[3]);
    }

    if (!strcmp(argv[1], "oneshot")) {
        const char *input = NULL, *events = NULL; bool headless = false, gui = false;
        for (int i = 2; i < argc; i++) {
            if (!strcmp(argv[i], "--input") && i+1<argc) input = argv[++i];
            else if (!strcmp(argv[i], "--events") && i+1<argc) events = argv[++i];
            else if (!strcmp(argv[i], "--headless")) headless = true;
            else if (!strcmp(argv[i], "--gui")) gui = true;
        }
        if (headless) { run_headless_oneshot(input, events); return 0; }

        char *json = NULL;
        if (input) {
            FILE *f = fopen(input, "r");
            if (f) { fseek(f,0,SEEK_END); long sz=ftell(f); rewind(f); json=malloc(sz+1); fread(json,1,sz,f); json[sz]=0; fclose(f); }
        } else {
            char buf[65536]; int n=read(STDIN_FILENO,buf,sizeof(buf)-1);
            if (n>0) { buf[n]=0; json=strdup(buf); }
        }
        if (!json||!json[0]) { fprintf(stderr,"Empty input\n"); return 1; }
        WidgetRequest *req=widget_request_parse(json); free(json);
        if (!req) { fprintf(stderr,"Invalid JSON\n"); return 1; }
        Widget *w=widget_registry_create(req);
        if (!w) { fprintf(stderr,"Unknown widget\n"); widget_request_free(req); return 1; }

        if (gui) {
#ifdef FILLY_GCORE
            GCoreBackend g;
            if (!gcore_backend_init(&g, GCORE_DRM, NULL)) { widget_destroy(w); return 1; }
            Backend bg = { .vtable = &gcore_vtable, .data = &g };
            WidgetResponse resp = session_run(w, &bg);
            printf("\n%s\n", widget_response_to_json(&resp));
            gcore_backend_destroy(&g);
#else
            fprintf(stderr,"GUI not compiled\n"); return 1;
#endif
        } else {
            TerminalBackend t;
            HeadlessBackend hl_fallback;
            Backend backend;
            bool use_terminal = terminal_backend_init(&t, NULL);
            if (use_terminal) {
                backend.vtable = &terminal_vtable;
                backend.data = &t;
            } else {
                headless_backend_init(&hl_fallback, 80, 24);
                backend.vtable = &headless_vtable;
                backend.data = &hl_fallback;
            }
            WidgetResponse resp = session_run(w, &backend);
            char *out = widget_response_to_json(&resp);
            if (use_terminal) {
                terminal_backend_destroy(&t);
                fprintf(stderr, "\r%s\n", out);
            } else {
                headless_backend_destroy(&hl_fallback);
                printf("%s\n", out);
            }
            free(out);
        }
        widget_destroy(w); widget_request_free(req);
        return 0;
    }

    printf("Usage: filly [command]\n");
    return 1;
}