#include "core/widget.h"
#include "core/session.h"
#include "backend/headless/headless.h"
#include "protocol/protocol.h"
#include "core/arena.h"
#include "core/store.h"
#include "cJSON.h"
#include "backend/daemon/daemon.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static double time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0;
}

static void benchmark(const char *name, const char *json, const char *events) {
    WidgetRequest *req = widget_request_parse(json);
    if (!req) { printf("%s,ERROR,0,0,0,0\n", name); return; }
    Widget *w = widget_registry_create(req);
    widget_request_free(req);
    if (!w) { printf("%s,ERROR,0,0,0,0\n", name); return; }

    HeadlessBackend hl;
    headless_backend_init(&hl, 80, 24);

    if (events) {
        const char *p = events;
        while (*p) {
            if (strncmp(p, "KEY:", 4) == 0) {
                p += 4;
                char k[32]; int i = 0;
                while (*p && *p != '\n' && i < 31) k[i++] = *p++;
                k[i] = 0;
                KeyCode c = KEY_NULL; char ch = 0;
                if (!strcmp(k,"ENTER")) c=KEY_ENTER;
                else if (!strcmp(k,"ESC")) c=KEY_ESC;
                else if (!strcmp(k,"DOWN")) c=KEY_DOWN;
                else if (!strcmp(k,"UP")) c=KEY_UP;
                else if (!strcmp(k,"TAB")) c=KEY_TAB;
                else if (!strcmp(k,"F1")) c=KEY_F1;
                else if (!strcmp(k,"SPACE")) { c=KEY_CHAR; ch=' '; }
                else if (strlen(k)==1) { c=KEY_CHAR; ch=k[0]; }
                if (c != KEY_NULL) headless_inject_key(&hl, c, ch);
            } else if (strncmp(p, "TEXT:", 5) == 0) {
                p += 5;
                while (*p && *p != '\n') { headless_inject_key(&hl, KEY_CHAR, *p); p++; }
            }
            while (*p && *p != '\n') p++;
            if (*p == '\n') p++;
        }
    }

    Backend backend = { .vtable = &headless_vtable, .data = &hl };

    double start = time_ms();
    WidgetResponse resp = session_run(w, &backend);
    double elapsed = time_ms() - start;

    extern Arena *g_session_arena;
    size_t arena_peak = g_session_arena ? g_session_arena->capacity : 0;

    printf("%s,%.2f,%zu,%d,%s\n", name, elapsed, arena_peak,
           resp.cancelled ? 1 : 0, resp.result ? "ok" : "null");

    widget_destroy(w);
    headless_backend_destroy(&hl);
}

int main(void) {
    set_insecure_plugins(true);
    register_builtin_widgets();
    load_plugins();

    printf("name,time_ms,arena_peak_bytes,cancelled,result\n");

    benchmark("msg_simple",
        "{\"widget\":\"msg\",\"params\":{\"title\":\"Test\",\"message\":\"Hello\"}}",
        "KEY:ENTER\n");

    benchmark("menu_1000",
        "{\"widget\":\"menu\",\"params\":{\"title\":\"Big\",\"choices\":["
        "\"A\",\"B\",\"C\",\"D\",\"E\",\"F\",\"G\",\"H\",\"I\",\"J\","
        "\"K\",\"L\",\"M\",\"N\",\"O\",\"P\",\"Q\",\"R\",\"S\",\"T\","
        "\"U\",\"V\",\"W\",\"X\",\"Y\",\"Z\",\"a\",\"b\",\"c\",\"d\","
        "\"e\",\"f\",\"g\",\"h\",\"i\",\"j\",\"k\",\"l\",\"m\",\"n\","
        "\"o\",\"p\",\"q\",\"r\",\"s\",\"t\",\"u\",\"v\",\"w\",\"x\","
        "\"y\",\"z\",\"0\",\"1\",\"2\",\"3\",\"4\",\"5\",\"6\",\"7\","
        "\"8\",\"9\",\"aa\",\"bb\",\"cc\",\"dd\",\"ee\",\"ff\",\"gg\","
        "\"hh\",\"ii\",\"jj\",\"kk\",\"ll\",\"mm\",\"nn\",\"oo\",\"pp\","
        "\"qq\",\"rr\",\"ss\",\"tt\",\"uu\",\"vv\",\"ww\",\"xx\",\"yy\","
        "\"zz\"]}}",
        "KEY:DOWN\nKEY:DOWN\nKEY:DOWN\nKEY:DOWN\nKEY:DOWN\nKEY:ENTER\n");

    benchmark("text_editor_10k",
        "{\"widget\":\"text_editor\",\"params\":{\"title\":\"Edit\",\"content\":\""
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\\n"
        "BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB\\n"
        "CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC\\n"
        "DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD\\n"
        "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\\n"
        "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF\\n"
        "GGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGG\\n"
        "HHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHH\\n"
        "IIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIII\\n"
        "JJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJ\"}}",
        "KEY:DOWN\nKEY:DOWN\nKEY:DOWN\nKEY:DOWN\nKEY:DOWN\nTEXT:X\nKEY:ESC\n");

    benchmark("hub_10x20",
        "{\"widget\":\"hub\",\"params\":{\"title\":\"Hub\",\"categories\":["
        "{\"label\":\"Cat1\",\"items\":["
        "{\"id\":\"k1\",\"label\":\"K1\",\"value\":\"v1\",\"widget\":\"input\"},"
        "{\"id\":\"k2\",\"label\":\"K2\",\"value\":\"v2\",\"widget\":\"input\"},"
        "{\"id\":\"k3\",\"label\":\"K3\",\"value\":\"v3\",\"widget\":\"input\"},"
        "{\"id\":\"k4\",\"label\":\"K4\",\"value\":\"v4\",\"widget\":\"input\"},"
        "{\"id\":\"k5\",\"label\":\"K5\",\"value\":\"v5\",\"widget\":\"input\"},"
        "{\"id\":\"k6\",\"label\":\"K6\",\"value\":\"v6\",\"widget\":\"input\"},"
        "{\"id\":\"k7\",\"label\":\"K7\",\"value\":\"v7\",\"widget\":\"input\"},"
        "{\"id\":\"k8\",\"label\":\"K8\",\"value\":\"v8\",\"widget\":\"input\"},"
        "{\"id\":\"k9\",\"label\":\"K9\",\"value\":\"v9\",\"widget\":\"input\"},"
        "{\"id\":\"k10\",\"label\":\"K10\",\"value\":\"v10\",\"widget\":\"input\"},"
        "{\"id\":\"k11\",\"label\":\"K11\",\"value\":\"v11\",\"widget\":\"input\"},"
        "{\"id\":\"k12\",\"label\":\"K12\",\"value\":\"v12\",\"widget\":\"input\"},"
        "{\"id\":\"k13\",\"label\":\"K13\",\"value\":\"v13\",\"widget\":\"input\"},"
        "{\"id\":\"k14\",\"label\":\"K14\",\"value\":\"v14\",\"widget\":\"input\"},"
        "{\"id\":\"k15\",\"label\":\"K15\",\"value\":\"v15\",\"widget\":\"input\"},"
        "{\"id\":\"k16\",\"label\":\"K16\",\"value\":\"v16\",\"widget\":\"input\"},"
        "{\"id\":\"k17\",\"label\":\"K17\",\"value\":\"v17\",\"widget\":\"input\"},"
        "{\"id\":\"k18\",\"label\":\"K18\",\"value\":\"v18\",\"widget\":\"input\"},"
        "{\"id\":\"k19\",\"label\":\"K19\",\"value\":\"v19\",\"widget\":\"input\"},"
        "{\"id\":\"k20\",\"label\":\"K20\",\"value\":\"v20\",\"widget\":\"input\"}"
        "]}],\"actions\":[\"Proceed\"]}}",
        "KEY:F1\nTEXT:y\n");

    benchmark("form_50_fields",
        "{\"widget\":\"form\",\"params\":{\"title\":\"F\",\"fields\":["
        "{\"label\":\"f0\",\"widget_type\":\"input\",\"value\":\"\"},"
        "{\"label\":\"f1\",\"widget_type\":\"input\",\"value\":\"\"},"
        "{\"label\":\"f2\",\"widget_type\":\"input\",\"value\":\"\"},"
        "{\"label\":\"f3\",\"widget_type\":\"input\",\"value\":\"\"},"
        "{\"label\":\"f4\",\"widget_type\":\"input\",\"value\":\"\"},"
        "{\"label\":\"f5\",\"widget_type\":\"input\",\"value\":\"\"},"
        "{\"label\":\"f6\",\"widget_type\":\"input\",\"value\":\"\"},"
        "{\"label\":\"f7\",\"widget_type\":\"input\",\"value\":\"\"},"
        "{\"label\":\"f8\",\"widget_type\":\"input\",\"value\":\"\"},"
        "{\"label\":\"f9\",\"widget_type\":\"input\",\"value\":\"\"}],\"submit_label\":\"OK\"}}",
        "KEY:TAB\nKEY:TAB\nKEY:TAB\nKEY:TAB\nKEY:TAB\nKEY:TAB\nKEY:TAB\nKEY:TAB\nKEY:TAB\nKEY:TAB\nKEY:ENTER\n");

    benchmark("yesno_quick",
        "{\"widget\":\"yesno\",\"params\":{\"title\":\"Q\",\"message\":\"M\",\"default\":true}}",
        "TEXT:y\n");

    return 0;
}