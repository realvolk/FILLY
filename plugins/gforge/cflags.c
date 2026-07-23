#include "core/widget.h"
#include "core/render.h"
#include "core/widget_base.h"
#include "core/session.h"
#include "protocol/protocol.h"
#include "cJSON.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct {
    WidgetBase base;
    char *title;
    char *cflags;
    char *cxxflags;
    char *makeopts;
    char *rustflags;
    int field;
} CFData;

extern Arena *g_session_arena;

static void cf_render(Widget *self, Rect area, RenderTree *out) {
    CFData *d = (CFData *)(self + 1);
    memset(out, 0, sizeof(*out));
    out->style_class = "container";
    int box_w = (int)(area.w * 0.6f), box_h = (int)(area.h * 0.5f);
    if (box_w > area.w - 2) box_w = area.w - 2;
    int box_x = (area.w - box_w) / 2, box_y = (area.h - box_h) / 2;
    const char *labels[] = {"CFLAGS", "CXXFLAGS", "MAKEOPTS", "RUSTFLAGS"};
    const char *vals[] = {d->cflags, d->cxxflags, d->makeopts, d->rustflags};
    RenderTree *c = arena_alloc(g_session_arena, 5 * sizeof(RenderTree));
    c[0].type = RNODE_TEXT; c[0].rect = rect_new(1, 0, box_w - 2, 1);
    c[0].text.content = arena_strdup(g_session_arena, d->title);
    c[0].style_class = "text"; c[0].state = "title";
    for (int i = 0; i < 4; i++) {
        c[1+i].type = RNODE_TEXT; c[1+i].rect = rect_new(1, 1+i, box_w - 2, 1);
        char buf[512]; snprintf(buf, sizeof(buf), "%s %s: %s", i == d->field ? ">" : " ", labels[i], vals[i]);
        c[1+i].text.content = arena_strdup(g_session_arena, buf);
        c[1+i].style_class = "text";
    }
    out->type = RNODE_CONTAINER; out->rect = rect_new(box_x, box_y, box_w, box_h);
    out->container.border = BORDER_SINGLE; out->container.padding = edgeinsets_zero();
    out->container.children = c; out->container.child_count = 5;
}

static EventResult cf_handle(Widget *self, Event *ev, Backend *backend) {
    (void)backend;
    CFData *d = (CFData *)(self + 1);
    if (ev->type != EVENT_KEY) return event_result_unhandled();
    switch (ev->code) {
        case KEY_ESC: return event_result_response((WidgetResponse){ .result = NULL, .cancelled = true });
        case KEY_UP: if (d->field > 0) d->field--; d->base.dirty = true; return event_result_handled();
        case KEY_DOWN: if (d->field < 3) d->field++; d->base.dirty = true; return event_result_handled();
        case KEY_F1: {
            cJSON *r = cJSON_CreateObject();
            cJSON_AddStringToObject(r, "CFLAGS", d->cflags);
            cJSON_AddStringToObject(r, "CXXFLAGS", d->cxxflags);
            cJSON_AddStringToObject(r, "MAKEOPTS", d->makeopts);
            cJSON_AddStringToObject(r, "RUSTFLAGS", d->rustflags);
            return event_result_response((WidgetResponse){ .result = r, .cancelled = false });
        }
        case KEY_ENTER: {
            const char *names[] = {"CFLAGS", "CXXFLAGS", "MAKEOPTS", "RUSTFLAGS"};
            return event_result_response((WidgetResponse){ .result = cJSON_CreateString(names[d->field]), .cancelled = false });
        }
        default: return event_result_unhandled();
    }
}

static void cf_destroy(Widget *w) {
    CFData *d = (CFData *)(w + 1);
    free(d->title); free(d->cflags); free(d->cxxflags); free(d->makeopts); free(d->rustflags);
}

Widget *cflags_factory(const WidgetRequest *req) {
    Widget *w = calloc(1, sizeof(Widget) + sizeof(CFData));
    CFData data;
    memset(&data, 0, sizeof(data));
    cJSON *title_j = cJSON_GetObjectItem(req->params, "title");
    cJSON *cflags_j = cJSON_GetObjectItem(req->params, "CFLAGS");
    cJSON *cxx_j = cJSON_GetObjectItem(req->params, "CXXFLAGS");
    cJSON *make_j = cJSON_GetObjectItem(req->params, "MAKEOPTS");
    cJSON *rust_j = cJSON_GetObjectItem(req->params, "RUSTFLAGS");
    data.title = strdup(title_j && title_j->valuestring ? title_j->valuestring : "CFLAGS");
    data.cflags = strdup(cflags_j && cflags_j->valuestring ? cflags_j->valuestring : "");
    data.cxxflags = strdup(cxx_j && cxx_j->valuestring ? cxx_j->valuestring : "");
    data.makeopts = strdup(make_j && make_j->valuestring ? make_j->valuestring : "");
    data.rustflags = strdup(rust_j && rust_j->valuestring ? rust_j->valuestring : "");
    data.field = 0;
    widget_base_init(w, &data, sizeof(CFData), cf_render, cf_handle, cf_destroy);
    return w;
}