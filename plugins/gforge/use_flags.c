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
    char **flags;
    int count;
    bool *selected;
    int cursor;
    int min, max;
} UFData;

extern Arena *g_session_arena;

static void uf_render(Widget *self, Rect area, RenderTree *out) {
    UFData *d = (UFData *)(self + 1);
    memset(out, 0, sizeof(*out));
    out->style_class = "container";
    int box_w = (int)(area.w * 0.7f), box_h = (int)(area.h * 0.8f);
    if (box_w > area.w - 2) box_w = area.w - 2;
    int box_x = (area.w - box_w) / 2, box_y = (area.h - box_h) / 2;
    RenderTree *c = arena_alloc(g_session_arena, 3 * sizeof(RenderTree));
    c[0].type = RNODE_TEXT; c[0].rect = rect_new(1, 0, box_w - 2, 1);
    c[0].text.content = arena_strdup(g_session_arena, d->title);
    c[0].style_class = "text"; c[0].state = "title";
    c[1].type = RNODE_LIST; c[1].rect = rect_new(1, 1, box_w - 2, box_h - 3);
    c[1].list.item_count = d->count; c[1].list.selected = d->cursor;
    c[1].list.items = arena_alloc(g_session_arena, d->count * sizeof(ListItem));
    for (int i = 0; i < d->count; i++) {
        char label[256];
        snprintf(label, sizeof(label), " %s %s", d->selected[i] ? "[x]" : "[ ]", d->flags[i]);
        c[1].list.items[i].label = arena_strdup(g_session_arena, label);
    }
    c[1].style_class = "list";
    c[2].type = RNODE_TEXT; c[2].rect = rect_new(1, box_h - 2, box_w - 2, 1);
    c[2].text.content = "Up/Down:move  Space:toggle  Enter:confirm  Esc:cancel";
    c[2].style_class = "text"; c[2].state = "muted";
    out->type = RNODE_CONTAINER; out->rect = rect_new(box_x, box_y, box_w, box_h);
    out->container.border = BORDER_SINGLE; out->container.padding = edgeinsets_zero();
    out->container.children = c; out->container.child_count = 3;
}

static EventResult uf_handle(Widget *self, Event *ev, Backend *backend) {
    (void)backend;
    UFData *d = (UFData *)(self + 1);
    if (ev->type != EVENT_KEY) return event_result_unhandled();
    switch (ev->code) {
        case KEY_ESC: return event_result_response((WidgetResponse){ .result = NULL, .cancelled = true });
        case KEY_UP: if (d->cursor > 0) d->cursor--; d->base.dirty = true; return event_result_handled();
        case KEY_DOWN: if (d->cursor + 1 < d->count) d->cursor++; d->base.dirty = true; return event_result_handled();
        case KEY_CHAR:
            if (ev->ch == ' ') {
                int sel = 0; for (int i = 0; i < d->count; i++) if (d->selected[i]) sel++;
                if (d->selected[d->cursor]) { if (sel > d->min) d->selected[d->cursor] = false; }
                else { if (sel < d->max) d->selected[d->cursor] = true; }
                d->base.dirty = true;
            }
            return event_result_handled();
        case KEY_ENTER: {
            char *result = strdup("");
            for (int i = 0; i < d->count; i++) {
                if (d->selected[i]) {
                    char *t = malloc(strlen(result) + strlen(d->flags[i]) + 2);
                    sprintf(t, "%s%s ", result, d->flags[i]);
                    free(result); result = t;
                }
            }
            if (strlen(result) > 0) result[strlen(result) - 1] = 0;
            cJSON *r = cJSON_CreateString(result);
            free(result);
            return event_result_response((WidgetResponse){ .result = r, .cancelled = false });
        }
        default: return event_result_unhandled();
    }
}

static void uf_destroy(Widget *w) {
    UFData *d = (UFData *)(w + 1);
    free(d->title);
    for (int i = 0; i < d->count; i++) free(d->flags[i]);
    free(d->flags); free(d->selected);
}

Widget *use_flags_factory(const WidgetRequest *req) {
    Widget *w = calloc(1, sizeof(Widget) + sizeof(UFData));
    UFData data;
    memset(&data, 0, sizeof(data));
    cJSON *title_j = cJSON_GetObjectItem(req->params, "title");
    data.title = strdup(title_j && title_j->valuestring ? title_j->valuestring : "USE Flags");
    cJSON *ch = cJSON_GetObjectItem(req->params, "choices");
    data.count = ch ? cJSON_GetArraySize(ch) : 0;
    data.flags = malloc(data.count * sizeof(char *));
    for (int i = 0; i < data.count; i++) data.flags[i] = strdup(cJSON_GetArrayItem(ch, i)->valuestring);
    data.selected = calloc(data.count, sizeof(bool));
    data.cursor = 0;
    cJSON *min_j = cJSON_GetObjectItem(req->params, "min");
    cJSON *max_j = cJSON_GetObjectItem(req->params, "max");
    data.min = min_j ? min_j->valueint : 0;
    data.max = max_j ? max_j->valueint : data.count;
    widget_base_init(w, &data, sizeof(UFData), uf_render, uf_handle, uf_destroy);
    return w;
}