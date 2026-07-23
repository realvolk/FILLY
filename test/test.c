#include "core/widget.h"
#include "core/session.h"
#include "backend/headless/headless.h"
#include "protocol/protocol.h"
#include "backend/daemon/daemon.h"
#include "core/widgets/menu.h"
#include "core/widgets/yesno.h"
#include "core/widgets/input.h"
#include "core/widgets/password.h"
#include "core/widgets/checklist.h"
#include "core/widgets/filter.h"
#include "core/widgets/toggle.h"
#include "core/widgets/range_slider.h"
#include "core/widgets/calendar.h"
#include "core/widgets/hub.h"
#include "core/widgets/tree.h"
#include "core/widgets/progress.h"
#include "core/widgets/msg.h"
#include "core/client.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include <unistd.h>
#include <time.h>

static int tests_run = 0, tests_passed = 0, tests_failed = 0;

static void test_fail(const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    fprintf(stderr, "  FAIL: "); vfprintf(stderr, fmt, ap); fprintf(stderr, "\n");
    va_end(ap); tests_failed++;
}
#define TEST(n) do { tests_run++; printf("  %s... ", n); } while(0)
#define CHECK(c) do { if (!(c)) { test_fail("%s:%d: %s", __FILE__, __LINE__, #c); return 1; } } while(0)
#define DONE() do { tests_passed++; printf("PASS\n"); } while(0)

static HeadlessBackend hl;
static Backend backend;
static void setup(void) { headless_backend_init(&hl, 80, 24); backend.vtable = &headless_vtable; backend.data = &hl; }

static void inject(const char *events) {
    const char *p = events;
    while (*p) {
        while (*p == '\n') p++;
        if (!*p) break;
        if (strncmp(p, "KEY:", 4) == 0) { p += 4;
            char k[32]; int i = 0;
            while (*p && *p != '\n' && i < 31) k[i++] = *p++;
            k[i] = 0;
            KeyCode c = KEY_NULL; char ch = 0;
            if (!strcmp(k,"UP")) c=KEY_UP; else if (!strcmp(k,"DOWN")) c=KEY_DOWN;
            else if (!strcmp(k,"LEFT")) c=KEY_LEFT; else if (!strcmp(k,"RIGHT")) c=KEY_RIGHT;
            else if (!strcmp(k,"ENTER")) c=KEY_ENTER; else if (!strcmp(k,"ESC")) c=KEY_ESC;
            else if (!strcmp(k,"TAB")) c=KEY_TAB; else if (!strcmp(k,"BACKSPACE")) c=KEY_BACKSPACE;
            else if (!strcmp(k,"HOME")) c=KEY_HOME; else if (!strcmp(k,"END")) c=KEY_END;
            else if (!strcmp(k,"SPACE")) { c=KEY_CHAR; ch=' '; }
            else if (!strcmp(k,"F1")) c=KEY_F1; else if (!strcmp(k,"F2")) c=KEY_F2;
            else if (!strcmp(k,"F3")) c=KEY_F3; else if (strlen(k)==1) { c=KEY_CHAR; ch=k[0]; }
            if (c != KEY_NULL) headless_inject_key(&hl, c, ch);
        } else if (strncmp(p, "TEXT:", 5) == 0) { p += 5;
            while (*p && *p != '\n') { headless_inject_key(&hl, KEY_CHAR, *p); p++; }
        } else if (strncmp(p, "WAIT:", 5) == 0) {
            p += 5;
            char ms_str[16]; int mi = 0;
            while (*p && *p != '\n' && mi < 15) ms_str[mi++] = *p++;
            ms_str[mi] = 0;
            int ms = atoi(ms_str);
            if (ms > 0) {
                struct timespec ts = {0, ms * 1000000L};
                nanosleep(&ts, NULL);
            }
        }
        while (*p && *p != '\n') p++;
        if (*p == '\n') p++;
    }
}

static WidgetResponse run_widget(Widget *w) {
    return session_run(w, &backend);
}

int main(void) {
    set_insecure_plugins(true);
    load_plugins();

    printf("=== FILLY C Test Suite ===\n\n");

    setup(); inject("KEY:ENTER\n");
    TEST("msg dismisses"); { Widget *w = msg_widget_new("T", "M"); WidgetResponse r = run_widget(w); CHECK(r.cancelled==false); widget_destroy(w); } DONE();

    setup(); inject("KEY:ENTER\n");
    TEST("menu default"); { char *choices[] = {"A","B"}; Widget *w = menu_widget_new("T", "", choices, 2, 0); WidgetResponse r = run_widget(w); CHECK(r.result&&!strcmp(r.result->valuestring,"A")); widget_destroy(w); } DONE();

    setup(); inject("KEY:ENTER\n");
    TEST("yesno default"); { Widget *w = yesno_widget_new("T", "", true); WidgetResponse r = run_widget(w); CHECK(r.result&&r.result->type==cJSON_True); widget_destroy(w); } DONE();

    setup(); inject("TEXT:hi\nKEY:ENTER\n");
    TEST("input text"); { Widget *w = input_widget_new("T", "", "", "", NULL); WidgetResponse r = run_widget(w); CHECK(r.result&&!strcmp(r.result->valuestring,"hi")); widget_destroy(w); } DONE();

    setup(); inject("TEXT:sec\nKEY:ENTER\n");
    TEST("password text"); { Widget *w = password_widget_new("T", "", ""); WidgetResponse r = run_widget(w); CHECK(r.result&&!strcmp(r.result->valuestring,"sec")); widget_destroy(w); } DONE();

    setup(); inject("KEY:SPACE\nKEY:DOWN\nKEY:SPACE\nKEY:ENTER\n");
    TEST("checklist toggle"); { char *choices[] = {"a","b","c"}; Widget *w = checklist_widget_new("T", "", choices, 3, 0, 3, NULL, 0); WidgetResponse r = run_widget(w); CHECK(r.result&&cJSON_GetArraySize(r.result)==2); widget_destroy(w); } DONE();

    setup(); inject("TEXT:Par\nKEY:ENTER\n");
    TEST("filter search"); { char *choices[] = {"London","Paris"}; Widget *w = filter_widget_new("T", "", choices, 2, NULL); WidgetResponse r = run_widget(w); CHECK(r.result&&!strcmp(r.result->valuestring,"Paris")); widget_destroy(w); } DONE();

    setup(); inject("KEY:SPACE\nKEY:ENTER\n");
    TEST("toggle on"); { Widget *w = toggle_widget_new("T", "L", false); WidgetResponse r = run_widget(w); CHECK(r.result&&r.result->type==cJSON_True); widget_destroy(w); } DONE();

    setup(); inject("KEY:RIGHT\nKEY:RIGHT\nKEY:ENTER\n");
    TEST("range slider"); { Widget *w = range_slider_widget_new("T", 0, 100, 50, ""); WidgetResponse r = run_widget(w); CHECK(r.result&&r.result->valueint==52); widget_destroy(w); } DONE();

    setup(); inject("KEY:ENTER\n");
    TEST("calendar date"); { Widget *w = calendar_widget_new("T"); WidgetResponse r = run_widget(w); CHECK(r.result&&r.result->valuestring&&strlen(r.result->valuestring)==10); widget_destroy(w); } DONE();

    setup(); inject("KEY:F1\nTEXT:y\n");
    TEST("hub proceed"); {
        cJSON *cat = cJSON_CreateArray();
        cJSON *cat_obj = cJSON_CreateObject();
        cJSON_AddStringToObject(cat_obj, "label", "C");
        cJSON *items = cJSON_CreateArray();
        cJSON *item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "id", "k");
        cJSON_AddStringToObject(item, "label", "K");
        cJSON_AddStringToObject(item, "value", "v");
        cJSON_AddStringToObject(item, "widget", "input");
        cJSON_AddItemToArray(items, item);
        cJSON_AddItemToObject(cat_obj, "items", items);
        cJSON_AddItemToArray(cat, cat_obj);
        cJSON *acts = cJSON_CreateStringArray((const char*[]){"Proceed"}, 1);
        Widget *w = hub_widget_new("H", cat, acts);
        WidgetResponse r = run_widget(w);
        CHECK(r.result);
        CHECK(!strcmp(cJSON_GetObjectItem(r.result,"k")->valuestring,"v"));
        widget_destroy(w);
        cJSON_Delete(cat);
        cJSON_Delete(acts);
    } DONE();

    setup(); inject("KEY:SPACE\nKEY:ESC\n");
    TEST("tree expand"); { TreeNode nodes[3] = {{.label="a",.expanded=false},{.label="b",.expanded=false},{.label="c",.expanded=false}}; Widget *w = tree_widget_new("T", nodes, 3); WidgetResponse r = run_widget(w); CHECK(r.cancelled==false); widget_destroy(w); } DONE();

    setup(); inject("KEY:ESC\n");
    TEST("progress cancel"); { char *cmd[] = {"sleep","10"}; Widget *w = progress_widget_new("T", cmd, 2, NULL); WidgetResponse r = run_widget(w); CHECK(r.cancelled==true); widget_destroy(w); } DONE();

    printf("\n=== Client Library Tests ===\n");
    TEST("connect invalid path");
    FillyClient *cl = filly_client_connect("/nonexistent/path");
    CHECK(cl == NULL);
    DONE();

    cl = filly_client_connect("/tmp/filly.sock");
    if (cl) {
        TEST("send_key with closed fd");
        close(filly_client_get_fd(cl));
        int ret = filly_client_send_key(cl, KEY_ENTER, 0);
        CHECK(ret < 0);
        filly_client_disconnect(cl);
        DONE();
    } else {
        printf("  send_key closed: SKIP (no daemon)\n");
    }

    printf("\n=== Results: %d/%d passed ===\n", tests_passed, tests_run);
    return tests_failed > 0 ? 1 : 0;
}