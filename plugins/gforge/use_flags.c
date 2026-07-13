#include "../../filly-core/widget.h"
#include "../../filly-core/render.h"
#include "../../filly-protocol/protocol.h"
#include "cJSON.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct { char *title; char **flags; int count; bool *selected; int cursor; int min, max; bool dirty; } UFData;

static void uf_render(Widget *self, Rect area, RenderTree *out) {
    UFData *d = (UFData *)(self + 1);
    memset(out, 0, sizeof(*out));
    int box_w = (int)(area.w * 0.7f), box_h = (int)(area.h * 0.8f);
    if (box_w > area.w - 2) box_w = area.w - 2;
    int box_x = (area.w - box_w) / 2, box_y = (area.h - box_h) / 2;
    RenderTree *c = calloc(3 + 1, sizeof(RenderTree));
    c[0].type = RNODE_TEXT; c[0].rect = rect_new(1, 0, box_w - 2, 1);
    c[0].text.content = strdup(d->title); c[0].text.align = ALIGN_CENTER; c[0].text.style = textstyle_selected();
    c[1].type = RNODE_LIST; c[1].rect = rect_new(1, 1, box_w - 2, box_h - 3);
    c[1].list.item_count = d->count; c[1].list.items = malloc(d->count * sizeof(ListItem));
    for (int i = 0; i < d->count; i++) {
        char label[256];
        snprintf(label, sizeof(label), " %s %s", d->selected[i] ? "[x]" : "[ ]", d->flags[i]);
        c[1].list.items[i] = listitem_new(label);
    }
    c[1].list.selected = d->cursor; c[1].list.highlight = textstyle_selected();
    c[2].type = RNODE_TEXT; c[2].rect = rect_new(1, box_h - 2, box_w - 2, 1);
    c[2].text.content = strdup("Up/Down:move  Space:toggle  Enter:confirm  Esc:cancel"); c[2].text.align = ALIGN_CENTER; c[2].text.style = textstyle_muted();
    out->type = RNODE_CONTAINER; out->rect = rect_new(box_x, box_y, box_w, box_h);
    out->container.border = BORDER_SINGLE; out->container.padding = edgeinsets_zero();
    out->container.children = c; out->container.child_count = 3;
}

static EventResult uf_handle(Widget *self, Event *ev, Backend *backend) {
    UFData *d = (UFData *)(self + 1);
    if (ev->type != EVENT_KEY) return event_result_unhandled();
    switch (ev->code) {
        case KEY_ESC: return event_result_response((WidgetResponse){ .result = NULL, .cancelled = true, .error = NULL });
        case KEY_UP: if (d->cursor > 0) d->cursor--; d->dirty = true; return event_result_handled();
        case KEY_DOWN: if (d->cursor + 1 < d->count) d->cursor++; d->dirty = true; return event_result_handled();
        case KEY_CHAR:
            if (ev->ch == ' ') {
                int sel = 0; for (int i = 0; i < d->count; i++) if (d->selected[i]) sel++;
                if (d->selected[d->cursor]) { if (sel > d->min) d->selected[d->cursor] = false; }
                else { if (sel < d->max) d->selected[d->cursor] = true; }
                d->dirty = true;
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
            return event_result_response((WidgetResponse){ .result = r, .cancelled = false, .error = NULL });
        }
        default: return event_result_unhandled();
    }
}

static bool uf_dirty(Widget *w) { return ((UFData *)(w + 1))->dirty; }
static void uf_clear(Widget *w) { ((UFData *)(w + 1))->dirty = false; }
static void uf_destroy(Widget *w) { UFData *d = (UFData *)(w + 1); free(d->title); for (int i = 0; i < d->count; i++) free(d->flags[i]); free(d->flags); free(d->selected); }

Widget *use_flags_factory(const WidgetRequest *req) {
    Widget *w = calloc(1, sizeof(Widget) + sizeof(UFData));
    w->vtable.render = uf_render; w->vtable.handle_event = uf_handle;
    w->vtable.is_dirty = uf_dirty; w->vtable.clear_dirty = uf_clear; w->vtable.destroy = uf_destroy;
    UFData *d = (UFData *)(w + 1);
    d->title = strdup(cJSON_GetObjectItem(req->params, "title")->valuestring ?: "USE Flags");
    cJSON *ch = cJSON_GetObjectItem(req->params, "choices");
    d->count = ch ? cJSON_GetArraySize(ch) : 0;
    d->flags = malloc(d->count * sizeof(char *));
    for (int i = 0; i < d->count; i++) d->flags[i] = strdup(cJSON_GetArrayItem(ch, i)->valuestring);
    d->selected = calloc(d->count, sizeof(bool));
    d->cursor = 0;
    d->min = cJSON_GetObjectItem(req->params, "min") ? cJSON_GetObjectItem(req->params, "min")->valueint : 0;
    d->max = cJSON_GetObjectItem(req->params, "max") ? cJSON_GetObjectItem(req->params, "max")->valueint : d->count;
    d->dirty = true;
    return w;
}