#include "../filly-core/widget.h"
#include "../filly-core/session.h"
#include "../filly-headless/headless.h"
#include "../filly-protocol/protocol.h"
#include "../filly-daemon/daemon.h"
#include "../filly-core/widgets/menu.h"
#include "../filly-core/widgets/yesno.h"
#include "../filly-core/widgets/input.h"
#include "../filly-core/widgets/password.h"
#include "../filly-core/widgets/checklist.h"
#include "../filly-core/widgets/filter.h"
#include "../filly-core/widgets/toggle.h"
#include "../filly-core/widgets/range_slider.h"
#include "../filly-core/widgets/calendar.h"
#include "../filly-core/widgets/hub.h"
#include "../filly-core/widgets/tree.h"
#include "../filly-core/widgets/progress.h"
#include "../filly-core/widgets/msg.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>

static int tests_run = 0, tests_passed = 0, tests_failed = 0;

static void test_fail(const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    fprintf(stderr, "  FAIL: "); vfprintf(stderr, fmt, ap); fprintf(stderr, "\n");
    va_end(ap); tests_failed++;
}
#define TEST(n) do { tests_run++; printf("  %s... ", n); } while(0)
#define CHECK(c) do { if (!(c)) { test_fail("%s:%d: %s", __FILE__, __LINE__, #c); return; } } while(0)
#define DONE() do { tests_passed++; printf("PASS\n"); } while(0)

static HeadlessBackend hl;
static Backend backend;
static void setup(void) { headless_backend_init(&hl, 80, 24); backend.vtable = &headless_vtable; backend.data = &hl; }
static void teardown(void) { headless_backend_destroy(&hl); }

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
        }
        while (*p && *p != '\n') p++;
        if (*p == '\n') p++;
    }
}

static WidgetResponse run(const char *widget, const char *params) {
    char full[65536];
    snprintf(full, sizeof(full), "{\"widget\":\"%s\",\"params\":%s}", widget, params);
    WidgetRequest *req = widget_request_parse(full);
    Widget *w = widget_registry_create(req);
    WidgetResponse r = session_run(w, &backend);
    widget_destroy(w); widget_request_free(req);
    return r;
}

static void reg(void) {
    widget_registry_register("menu", menu_widget_factory);
    widget_registry_register("yesno", yesno_widget_factory);
    widget_registry_register("input", input_widget_factory);
    widget_registry_register("password", password_widget_factory);
    widget_registry_register("checklist", checklist_widget_factory);
    widget_registry_register("filter", filter_widget_factory);
    widget_registry_register("toggle", toggle_widget_factory);
    widget_registry_register("range_slider", range_slider_widget_factory);
    widget_registry_register("calendar", calendar_widget_factory);
    widget_registry_register("hub", hub_widget_factory);
    widget_registry_register("tree", tree_widget_factory);
    widget_registry_register("progress", progress_widget_factory);
    widget_registry_register("msg", msg_widget_factory);
}

int main(void) {
    set_insecure_plugins(true);
    reg();
    load_plugins();

    printf("=== FILLY C Test Suite ===\n\n");

    setup(); inject("KEY:ENTER\n");
    TEST("msg dismisses"); CHECK(run("msg","{\"title\":\"T\",\"message\":\"M\"}").cancelled==false); DONE();
    setup(); inject("KEY:ENTER\n");
    TEST("menu default"); { WidgetResponse r=run("menu","{\"title\":\"T\",\"choices\":[\"A\",\"B\"]}"); CHECK(r.result&&!strcmp(r.result->valuestring,"A")); } DONE();
    setup(); inject("KEY:ENTER\n");
    TEST("yesno default"); { WidgetResponse r=run("yesno","{\"title\":\"T\",\"default\":true}"); CHECK(r.result&&r.result->type==cJSON_True); } DONE();
    setup(); inject("TEXT:hi\nKEY:ENTER\n");
    TEST("input text"); { WidgetResponse r=run("input","{\"title\":\"T\"}"); CHECK(r.result&&!strcmp(r.result->valuestring,"hi")); } DONE();
    setup(); inject("TEXT:sec\nKEY:ENTER\n");
    TEST("password text"); { WidgetResponse r=run("password","{\"title\":\"T\"}"); CHECK(r.result&&!strcmp(r.result->valuestring,"sec")); } DONE();
    setup(); inject("KEY:SPACE\nKEY:DOWN\nKEY:SPACE\nKEY:ENTER\n");
    TEST("checklist toggle"); { WidgetResponse r=run("checklist","{\"title\":\"T\",\"choices\":[\"a\",\"b\",\"c\"]}"); CHECK(r.result&&cJSON_GetArraySize(r.result)==2); } DONE();
    setup(); inject("TEXT:Par\nKEY:ENTER\n");
    TEST("filter search"); { WidgetResponse r=run("filter","{\"title\":\"T\",\"choices\":[\"London\",\"Paris\"]}"); CHECK(r.result&&!strcmp(r.result->valuestring,"Paris")); } DONE();
    setup(); inject("KEY:SPACE\nKEY:ENTER\n");
    TEST("toggle on"); { WidgetResponse r=run("toggle","{\"title\":\"T\",\"label\":\"L\",\"default\":false}"); CHECK(r.result&&r.result->type==cJSON_True); } DONE();
    setup(); inject("KEY:RIGHT\nKEY:RIGHT\nKEY:ENTER\n");
    TEST("range slider"); { WidgetResponse r=run("range_slider","{\"title\":\"T\",\"min\":0,\"max\":100,\"value\":50}"); CHECK(r.result&&r.result->valueint==52); } DONE();
    setup(); inject("KEY:ENTER\n");
    TEST("calendar date"); { WidgetResponse r=run("calendar","{\"title\":\"T\"}"); CHECK(r.result&&r.result->valuestring&&strlen(r.result->valuestring)==10); } DONE();
    setup(); inject("KEY:ENTER\nTEXT:new\nKEY:ENTER\nKEY:F1\nTEXT:y\n");
    TEST("hub edit"); { WidgetResponse r=run("hub","{\"title\":\"H\",\"categories\":[{\"label\":\"C\",\"items\":[{\"id\":\"k\",\"label\":\"K\",\"value\":\"v\",\"widget\":\"input\"}]}],\"actions\":[\"Proceed\"]}"); CHECK(r.result); CHECK(!strcmp(cJSON_GetObjectItem(r.result,"k")->valuestring,"vnew")); } DONE();
    setup(); inject("KEY:SPACE\nKEY:ESC\n");
    TEST("tree expand"); { WidgetResponse r=run("tree","{\"title\":\"T\"}"); CHECK(r.cancelled==false); } DONE();
    setup(); inject("KEY:ESC\n");
    TEST("progress cancel"); { WidgetResponse r=run("progress","{\"title\":\"T\",\"command\":[\"sleep\",\"10\"]}"); CHECK(r.cancelled==true); } DONE();

    printf("\n=== Results: %d/%d passed ===\n", tests_passed, tests_run);
    return tests_failed > 0 ? 1 : 0;
}