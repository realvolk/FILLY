#include <stdio.h>
#include "../../filly-core/widget.h"
#include "../../filly-core/render.h"
#include "../../filly-protocol/protocol.h"
#include "cJSON.h"
#include <stdlib.h>
#include <string.h>

typedef struct { char *title; char *cflags; char *cxxflags; char *makeopts; char *rustflags; int field; bool dirty; } CFData;

static void cf_render(Widget *self, Rect area, RenderTree *out) {
    CFData *d = (CFData *)(self + 1);
    memset(out, 0, sizeof(*out));
    int box_w = (int)(area.w * 0.6f), box_h = (int)(area.h * 0.5f);
    if (box_w > area.w - 2) box_w = area.w - 2;
    int box_x = (area.w - box_w) / 2, box_y = (area.h - box_h) / 2;
    const char *labels[] = {"CFLAGS", "CXXFLAGS", "MAKEOPTS", "RUSTFLAGS"};
    const char *vals[] = {d->cflags, d->cxxflags, d->makeopts, d->rustflags};
    RenderTree *c = calloc(5 + 1, sizeof(RenderTree));
    c[0].type = RNODE_TEXT; c[0].rect = rect_new(1, 0, box_w - 2, 1);
    c[0].text.content = strdup(d->title); c[0].text.align = ALIGN_CENTER; c[0].text.style = textstyle_selected();
    for (int i = 0; i < 4; i++) {
        c[1+i].type = RNODE_TEXT; c[1+i].rect = rect_new(1, 1+i, box_w - 2, 1);
        char buf[512]; snprintf(buf, sizeof(buf), "%s %s: %s", i == d->field ? ">" : " ", labels[i], vals[i]);
        c[1+i].text.content = strdup(buf); c[1+i].text.align = ALIGN_LEFT;
        c[1+i].text.style = i == d->field ? textstyle_selected() : textstyle_normal();
    }
    out->type = RNODE_CONTAINER; out->rect = rect_new(box_x, box_y, box_w, box_h);
    out->container.border = BORDER_SINGLE; out->container.padding = edgeinsets_zero();
    out->container.children = c; out->container.child_count = 5;
}

static EventResult cf_handle(Widget *self, Event *ev, Backend *backend) {
    CFData *d = (CFData *)(self + 1);
    if (ev->type != EVENT_KEY) return event_result_unhandled();
    switch (ev->code) {
        case KEY_ESC: return event_result_response((WidgetResponse){ .result = NULL, .cancelled = true, .error = NULL });
        case KEY_UP: if (d->field > 0) d->field--; d->dirty = true; return event_result_handled();
        case KEY_DOWN: if (d->field < 3) d->field++; d->dirty = true; return event_result_handled();
        case KEY_F1: {
            cJSON *r = cJSON_CreateObject();
            cJSON_AddStringToObject(r, "CFLAGS", d->cflags);
            cJSON_AddStringToObject(r, "CXXFLAGS", d->cxxflags);
            cJSON_AddStringToObject(r, "MAKEOPTS", d->makeopts);
            cJSON_AddStringToObject(r, "RUSTFLAGS", d->rustflags);
            return event_result_response((WidgetResponse){ .result = r, .cancelled = false, .error = NULL });
        }
        case KEY_ENTER: {
            const char *names[] = {"CFLAGS", "CXXFLAGS", "MAKEOPTS", "RUSTFLAGS"};
            return event_result_response((WidgetResponse){ .result = cJSON_CreateString(names[d->field]), .cancelled = false, .error = NULL });
        }
        default: return event_result_unhandled();
    }
}

static bool cf_dirty(Widget *w) { return ((CFData *)(w + 1))->dirty; }
static void cf_clear(Widget *w) { ((CFData *)(w + 1))->dirty = false; }
static void cf_destroy(Widget *w) { CFData *d = (CFData *)(w + 1); free(d->title); free(d->cflags); free(d->cxxflags); free(d->makeopts); free(d->rustflags); }

Widget *cflags_factory(const WidgetRequest *req) {
    Widget *w = calloc(1, sizeof(Widget) + sizeof(CFData));
    w->vtable.render = cf_render; w->vtable.handle_event = cf_handle;
    w->vtable.is_dirty = cf_dirty; w->vtable.clear_dirty = cf_clear; w->vtable.destroy = cf_destroy;
    CFData *d = (CFData *)(w + 1);
    d->title = strdup(cJSON_GetObjectItem(req->params, "title")->valuestring ?: "CFLAGS");
    d->cflags = strdup(cJSON_GetObjectItem(req->params, "CFLAGS")->valuestring ?: "");
    d->cxxflags = strdup(cJSON_GetObjectItem(req->params, "CXXFLAGS")->valuestring ?: "");
    d->makeopts = strdup(cJSON_GetObjectItem(req->params, "MAKEOPTS")->valuestring ?: "");
    d->rustflags = strdup(cJSON_GetObjectItem(req->params, "RUSTFLAGS")->valuestring ?: "");
    d->field = 0; d->dirty = true;
    return w;
}