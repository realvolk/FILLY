#include "../../filly-core/widget.h"
#include "../../filly-core/render.h"
#include "../../filly-protocol/protocol.h"
#include "cJSON.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct {
    char *title;
    char *current_init;
    char **targets;
    int target_count;
    int source_idx;
    int target_idx;
    int field;
    bool dirty;
} MigInitData;

static void mi_render(Widget *self, Rect area, RenderTree *out) {
    MigInitData *d = (MigInitData *)(self + 1);
    memset(out, 0, sizeof(*out));
    int box_w = (int)(area.w * 0.50f), box_h = (int)(area.h * 0.35f);
    if (box_w > area.w - 2) box_w = area.w - 2;
    int box_x = (area.w - box_w) / 2, box_y = (area.h - box_h) / 2;

    RenderTree *children = calloc(3 + 1, sizeof(RenderTree));
    children[0].type = RNODE_TEXT;
    children[0].rect = rect_new(1, 0, box_w - 2, 1);
    children[0].text.content = strdup(d->title);
    children[0].text.align = ALIGN_CENTER;
    children[0].text.style = textstyle_selected();

    children[1].type = RNODE_TEXT;
    children[1].rect = rect_new(1, 1, box_w - 2, 1);
    char buf[256];
    snprintf(buf, sizeof(buf), "%s Source: %s", d->field == 0 ? ">" : " ", d->current_init);
    children[1].text.content = strdup(buf);
    children[1].text.align = ALIGN_LEFT;
    children[1].text.style = d->field == 0 ? textstyle_selected() : textstyle_normal();

    children[2].type = RNODE_TEXT;
    children[2].rect = rect_new(1, 2, box_w - 2, 1);
    snprintf(buf, sizeof(buf), "%s Target: %s", d->field == 1 ? ">" : " ", d->targets[d->target_idx]);
    children[2].text.content = strdup(buf);
    children[2].text.align = ALIGN_LEFT;
    children[2].text.style = d->field == 1 ? textstyle_selected() : textstyle_normal();

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
        case KEY_ESC: return event_result_response((WidgetResponse){ .result = NULL, .cancelled = true, .error = NULL });
        case KEY_UP: d->field = d->field > 0 ? d->field - 1 : 0; d->dirty = true; return event_result_handled();
        case KEY_DOWN: if (d->field < 1) d->field++; d->dirty = true; return event_result_handled();
        case KEY_LEFT:
            if (d->field == 1 && d->target_idx > 0) d->target_idx--;
            d->dirty = true; return event_result_handled();
        case KEY_RIGHT:
            if (d->field == 1 && d->target_idx + 1 < d->target_count) d->target_idx++;
            d->dirty = true; return event_result_handled();
        case KEY_ENTER:
            if (strcmp(d->current_init, d->targets[d->target_idx]) == 0) return event_result_handled();
            cJSON *r = cJSON_CreateObject();
            cJSON_AddStringToObject(r, "source", d->current_init);
            cJSON_AddStringToObject(r, "target", d->targets[d->target_idx]);
            return event_result_response((WidgetResponse){ .result = r, .cancelled = false, .error = NULL });
        default: return event_result_unhandled();
    }
}

static bool mi_is_dirty(Widget *self) { return ((MigInitData *)(self + 1))->dirty; }
static void mi_clear_dirty(Widget *self) { ((MigInitData *)(self + 1))->dirty = false; }
static void mi_destroy(Widget *self) {
    MigInitData *d = (MigInitData *)(self + 1);
    free(d->title); free(d->current_init);
    for (int i = 0; i < d->target_count; i++) free(d->targets[i]);
    free(d->targets);
}

Widget *migration_init_factory(const WidgetRequest *req) {
    Widget *w = calloc(1, sizeof(Widget) + sizeof(MigInitData));
    w->vtable.render = mi_render; w->vtable.handle_event = mi_handle;
    w->vtable.is_dirty = mi_is_dirty; w->vtable.clear_dirty = mi_clear_dirty; w->vtable.destroy = mi_destroy;
    MigInitData *d = (MigInitData *)(w + 1);
    cJSON *title_j = cJSON_GetObjectItem(req->params, "title");
    cJSON *init_j = cJSON_GetObjectItem(req->params, "current_init");
    d->title = strdup(title_j && title_j->valuestring ? title_j->valuestring : "Init Migration");
    d->current_init = strdup(init_j && init_j->valuestring ? init_j->valuestring : "openrc");
    d->targets = malloc(4 * sizeof(char *));
    d->targets[0] = strdup("openrc"); d->targets[1] = strdup("runit");
    d->targets[2] = strdup("dinit"); d->targets[3] = strdup("s6");
    d->target_count = 4;
    d->source_idx = 0; d->target_idx = 0; d->field = 1; d->dirty = true;
    return w;
}