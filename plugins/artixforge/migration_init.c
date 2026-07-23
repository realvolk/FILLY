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
    char *current_init;
    char **targets;
    int target_count;
    int target_idx;
    int field;
} MigInitData;

extern Arena *g_session_arena;

static void mi_render(Widget *self, Rect area, RenderTree *out) {
    MigInitData *d = (MigInitData *)(self + 1);
    memset(out, 0, sizeof(*out));
    out->style_class = "container";
    int box_w = (int)(area.w * 0.50f), box_h = (int)(area.h * 0.35f);
    if (box_w > area.w - 2) box_w = area.w - 2;
    int box_x = (area.w - box_w) / 2, box_y = (area.h - box_h) / 2;

    RenderTree *children = arena_alloc(g_session_arena, 3 * sizeof(RenderTree));
    children[0].type = RNODE_TEXT;
    children[0].rect = rect_new(1, 0, box_w - 2, 1);
    children[0].text.content = arena_strdup(g_session_arena, d->title);
    children[0].style_class = "text"; children[0].state = "title";

    children[1].type = RNODE_TEXT;
    children[1].rect = rect_new(1, 1, box_w - 2, 1);
    char buf[256];
    snprintf(buf, sizeof(buf), "%s Source: %s", d->field == 0 ? ">" : " ", d->current_init);
    children[1].text.content = arena_strdup(g_session_arena, buf);
    children[1].style_class = "text";

    children[2].type = RNODE_TEXT;
    children[2].rect = rect_new(1, 2, box_w - 2, 1);
    snprintf(buf, sizeof(buf), "%s Target: %s", d->field == 1 ? ">" : " ", d->targets[d->target_idx]);
    children[2].text.content = arena_strdup(g_session_arena, buf);
    children[2].style_class = "text";

    out->type = RNODE_CONTAINER;
    out->rect = rect_new(box_x, box_y, box_w, box_h);
    out->container.border = BORDER_SINGLE;
    out->container.padding = edgeinsets_zero();
    out->container.children = children;
    out->container.child_count = 3;
}

static EventResult mi_handle(Widget *self, Event *ev, Backend *backend) {
    (void)backend;
    MigInitData *d = (MigInitData *)(self + 1);
    if (ev->type != EVENT_KEY) return event_result_unhandled();
    switch (ev->code) {
        case KEY_ESC: return event_result_response((WidgetResponse){ .result = NULL, .cancelled = true });
        case KEY_UP: d->field = d->field > 0 ? d->field - 1 : 0; d->base.dirty = true; return event_result_handled();
        case KEY_DOWN: if (d->field < 1) d->field++; d->base.dirty = true; return event_result_handled();
        case KEY_LEFT:
            if (d->field == 1 && d->target_idx > 0) d->target_idx--;
            d->base.dirty = true; return event_result_handled();
        case KEY_RIGHT:
            if (d->field == 1 && d->target_idx + 1 < d->target_count) d->target_idx++;
            d->base.dirty = true; return event_result_handled();
        case KEY_ENTER:
            if (strcmp(d->current_init, d->targets[d->target_idx]) == 0) return event_result_handled();
            cJSON *r = cJSON_CreateObject();
            cJSON_AddStringToObject(r, "source", d->current_init);
            cJSON_AddStringToObject(r, "target", d->targets[d->target_idx]);
            return event_result_response((WidgetResponse){ .result = r, .cancelled = false });
        default: return event_result_unhandled();
    }
}

static void mi_destroy(Widget *self) {
    MigInitData *d = (MigInitData *)(self + 1);
    free(d->title); free(d->current_init);
    for (int i = 0; i < d->target_count; i++) free(d->targets[i]);
    free(d->targets);
}

Widget *migration_init_factory(const WidgetRequest *req) {
    Widget *w = calloc(1, sizeof(Widget) + sizeof(MigInitData));
    MigInitData data;
    memset(&data, 0, sizeof(data));
    cJSON *title_j = cJSON_GetObjectItem(req->params, "title");
    cJSON *init_j = cJSON_GetObjectItem(req->params, "current_init");
    data.title = strdup(title_j && title_j->valuestring ? title_j->valuestring : "Init Migration");
    data.current_init = strdup(init_j && init_j->valuestring ? init_j->valuestring : "openrc");
    data.targets = malloc(4 * sizeof(char *));
    data.targets[0] = strdup("openrc"); data.targets[1] = strdup("runit");
    data.targets[2] = strdup("dinit"); data.targets[3] = strdup("s6");
    data.target_count = 4;
    data.target_idx = 0; data.field = 1;
    widget_base_init(w, &data, sizeof(MigInitData), mi_render, mi_handle, mi_destroy);
    return w;
}