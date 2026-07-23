#include "core/session.h"
#include "core/widget_base.h"
#include "core/render.h"
#include "backend/headless/headless.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>

typedef struct {
    WidgetBase base;
    int value;
} TestWidget;

static void test_render(Widget *self, Rect area, RenderTree *out) {
    TestWidget *d = (TestWidget *)(self + 1);
    memset(out, 0, sizeof(*out));
    out->type = RNODE_TEXT;
    out->rect = area;
    out->text.content = strdup(d->value ? "yes" : "no");
    out->style_class = "text";
}

static EventResult test_handle(Widget *self, Event *ev, Backend *backend) {
    (void)backend;
    TestWidget *d = (TestWidget *)(self + 1);
    if (ev->type != EVENT_KEY) return event_result_unhandled();
    if (ev->code == KEY_ENTER)
        return event_result_response((WidgetResponse){ .result = cJSON_CreateNumber(d->value), .cancelled = false });
    if (ev->code == KEY_CHAR && ev->ch == 't') {
        d->value = !d->value;
        d->base.dirty = true;
        return event_result_handled();
    }
    return event_result_unhandled();
}

static void test_destroy(Widget *self) { (void)self; }

int main(void) {
    printf("=== Session Tests ===\n");

    Widget *w = calloc(1, sizeof(Widget) + sizeof(TestWidget));
    TestWidget data = { .value = 0 };
    widget_base_init(w, &data, sizeof(TestWidget), test_render, test_handle, test_destroy);

    HeadlessBackend hl;
    headless_backend_init(&hl, 80, 24);
    Backend backend = { .vtable = &headless_vtable, .data = &hl };

    headless_inject_key(&hl, KEY_ENTER, 0);
    WidgetResponse r = session_run(w, &backend);
    assert(r.cancelled == false);
    assert(r.result->valueint == 0);

    headless_backend_destroy(&hl);
    widget_destroy(w);
    printf("PASS: All session tests\n");
    return 0;
}