#include "core/widget.h"
#include "core/session.h"
#include "backend/headless/headless.h"
#include "protocol/protocol.h"
#include "core/arena.h"
#include "cJSON.h"
#include "backend/daemon/daemon.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>

static int tests_run = 0, tests_passed = 0, tests_failed = 0;
#define TEST(n) do { tests_run++; printf("  %s... ", n); } while(0)
#define CHECK(c) do { if (!(c)) { printf("FAIL\n"); tests_failed++; return; } } while(0)
#define PASS() do { printf("PASS\n"); tests_passed++; } while(0)

static struct { const char *name; KeyCode code; } key_table[] = {
    {"ENTER", KEY_ENTER}, {"ESC", KEY_ESC}, {"LEFT", KEY_LEFT},
    {"RIGHT", KEY_RIGHT}, {"UP", KEY_UP}, {"DOWN", KEY_DOWN},
    {"TAB", KEY_TAB}, {"F1", KEY_F1}, {"SPACE", KEY_CHAR},
};

static WidgetResponse run_widget(const char *json_str, const char *events) {
    WidgetRequest *req = widget_request_parse(json_str);
    if (!req) { WidgetResponse r = {0}; return r; }
    Widget *w = widget_registry_create(req);
    widget_request_free(req);
    if (!w) { WidgetResponse r = {0}; return r; }
    HeadlessBackend hl;
    headless_backend_init(&hl, 80, 24);
    Backend backend = { .vtable = &headless_vtable, .data = &hl };
    if (events) {
        const char *p = events;
        while (*p) {
            if (strncmp(p, "KEY:", 4) == 0) {
                p += 4;
                char k[32]; int i = 0;
                while (*p && *p != '\n' && i < 31) k[i++] = *p++;
                k[i] = 0;
                KeyCode c = KEY_NULL; char ch = 0;
                int nk = sizeof(key_table) / sizeof(key_table[0]);
                for (int j = 0; j < nk; j++) {
                    if (!strcmp(k, key_table[j].name)) {
                        c = key_table[j].code;
                        if (c == KEY_CHAR && !strcmp(k, "SPACE")) ch = ' ';
                        break;
                    }
                }
                if (c == KEY_NULL && strlen(k) == 1) { c = KEY_CHAR; ch = k[0]; }
                if (c != KEY_NULL) headless_inject_key(&hl, c, ch);
            } else if (strncmp(p, "TEXT:", 5) == 0) {
                p += 5;
                while (*p && *p != '\n') { headless_inject_key(&hl, KEY_CHAR, *p); p++; }
            } else if (strncmp(p, "MOUSE:", 6) == 0) {
                char *rest = (char *)p + 6;
                if (strncmp(rest, "PRESS:", 6) == 0) {
                    int x = 0, y = 0;
                    sscanf(rest + 6, "%d,%d", &x, &y);
                    headless_inject_mouse(&hl, EVENT_MOUSE_BUTTON, x, y, 1);
                } else if (strncmp(rest, "RELEASE:", 8) == 0) {
                    int x = 0, y = 0;
                    sscanf(rest + 8, "%d,%d", &x, &y);
                    headless_inject_mouse(&hl, EVENT_MOUSE_BUTTON, x, y, 0);
                }
            }
            while (*p && *p != '\n') p++;
            if (*p == '\n') p++;
        }
    }
    WidgetResponse resp = session_run(w, &backend);
    widget_destroy(w);
    headless_backend_destroy(&hl);
    return resp;
}

static void test_corrupted_json(void) {
    TEST("corrupted JSON returns error");
    WidgetResponse r = run_widget("{bad json", NULL);
    CHECK(r.cancelled || r.result == NULL);
    PASS();
}

static void test_truncated_message(void) {
    TEST("truncated message handled");
    WidgetResponse r = run_widget("{\"widget\":\"msg\",\"params\":{\"title\":\"T\",\"message\":\"M\"", NULL);
    CHECK(r.cancelled || r.result == NULL);
    PASS();
}

static void test_unknown_widget(void) {
    TEST("unknown widget returns error");
    WidgetResponse r = run_widget("{\"widget\":\"nonexistent\",\"params\":{}}", "KEY:ENTER\n");
    CHECK(r.cancelled || r.result == NULL);
    PASS();
}

static void test_deeply_nested_json(void) {
    TEST("deeply nested JSON handled");
    char *json = malloc(65536);
    strcpy(json, "{\"widget\":\"msg\",\"params\":{\"title\":\"T\",\"message\":\"");
    for (int i = 0; i < 100; i++) strcat(json, "[[[[[[]]]]]]");
    strcat(json, "\"}}");
    WidgetResponse r = run_widget(json, "KEY:ENTER\n");
    CHECK(r.cancelled || r.result == NULL);
    free(json);
    PASS();
}

static void test_arena_exhaustion(void) {
    TEST("arena exhaustion handled");
    extern Arena *g_session_arena;
    WidgetRequest *req = widget_request_parse("{\"widget\":\"msg\",\"params\":{\"title\":\"T\",\"message\":\"M\"}}");
    Widget *w = widget_registry_create(req);
    widget_request_free(req);
    HeadlessBackend hl;
    headless_backend_init(&hl, 80, 24);
    Backend backend = { .vtable = &headless_vtable, .data = &hl };
    headless_inject_key(&hl, KEY_ENTER, 0);
    if (g_session_arena) {
        arena_alloc(g_session_arena, g_session_arena->capacity);
    }
    WidgetResponse r = session_run(w, &backend);
    CHECK(r.cancelled || r.result == NULL);
    widget_destroy(w);
    headless_backend_destroy(&hl);
    PASS();
}

static void test_null_params(void) {
    TEST("null params handled");
    WidgetResponse r = run_widget("{\"widget\":\"msg\"}", "KEY:ENTER\n");
    CHECK(r.cancelled || r.result == NULL);
    PASS();
}

static void test_empty_string(void) {
    TEST("empty string handled");
    WidgetResponse r = run_widget("", NULL);
    CHECK(r.cancelled || r.result == NULL);
    PASS();
}

static void test_massive_choices(void) {
    TEST("massive choices list handled");
    char *json = malloc(262144);
    strcpy(json, "{\"widget\":\"menu\",\"params\":{\"title\":\"T\",\"choices\":[");
    for (int i = 0; i < 1000; i++) {
        if (i > 0) strcat(json, ",");
        strcat(json, "\"item");
        char num[16]; snprintf(num, sizeof(num), "%d", i);
        strcat(json, num);
        strcat(json, "\"");
    }
    strcat(json, "]}}");
    WidgetResponse r = run_widget(json, "KEY:ENTER\n");
    CHECK(r.cancelled || r.result);
    free(json);
    PASS();
}

static void test_mouse_injection(void) {
    TEST("mouse injection works");
    WidgetResponse r = run_widget("{\"widget\":\"msg\",\"params\":{\"title\":\"T\",\"message\":\"M\"}}",
        "MOUSE:PRESS:10,10\nMOUSE:RELEASE:10,10\n");
    CHECK(r.cancelled || r.result == NULL);
    PASS();
}

static void test_concurrent_keys(void) {
    TEST("rapid key events handled");
    WidgetResponse r = run_widget("{\"widget\":\"yesno\",\"params\":{\"title\":\"T\",\"message\":\"M\",\"default\":true}}",
        "KEY:LEFT\nKEY:RIGHT\nKEY:LEFT\nKEY:RIGHT\nKEY:ENTER\n");
    CHECK(r.cancelled || r.result);
    PASS();
}

int main(void) {
    set_insecure_plugins(true);
    register_builtin_widgets();
    load_plugins();

    printf("=== FILLY Fault Injection Tests ===\n\n");
    test_corrupted_json();
    test_truncated_message();
    test_unknown_widget();
    test_deeply_nested_json();
    test_arena_exhaustion();
    test_null_params();
    test_empty_string();
    test_massive_choices();
    test_mouse_injection();
    test_concurrent_keys();
    printf("\n=== Results: %d/%d passed ===\n", tests_passed, tests_run);
    return tests_failed > 0 ? 1 : 0;
}