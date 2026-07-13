#include "../../filly-core/widget.h"
#include "../../filly-core/render.h"
#include "../../filly-protocol/protocol.h"
#include "cJSON.h"
#include <stdlib.h>
#include <string.h>

typedef struct { char *title; char **choices; int count; int selected; bool dirty; } ProfData;

static void prof_render(Widget *self, Rect area, RenderTree *out) {
    ProfData *d = (ProfData *)(self + 1);
    memset(out, 0, sizeof(*out));
    int box_w = (int)(area.w * 0.6f), box_h = (int)(area.h * 0.7f);
    if (box_w > area.w - 2) box_w = area.w - 2;
    int box_x = (area.w - box_w) / 2, box_y = (area.h - box_h) / 2;
    RenderTree *c = calloc(3 + 1, sizeof(RenderTree));
    c[0].type = RNODE_TEXT; c[0].rect = rect_new(1, 0, box_w - 2, 1);
    c[0].text.content = strdup(d->title); c[0].text.align = ALIGN_CENTER; c[0].text.style = textstyle_selected();
    c[1].type = RNODE_LIST; c[1].rect = rect_new(1, 1, box_w - 2, box_h - 3);
    c[1].list.item_count = d->count; c[1].list.items = malloc(d->count * sizeof(ListItem));
    for (int i = 0; i < d->count; i++) c[1].list.items[i] = listitem_new(d->choices[i]);
    c[1].list.selected = d->selected; c[1].list.highlight = textstyle_selected();
    c[2].type = RNODE_TEXT; c[2].rect = rect_new(1, box_h - 2, box_w - 2, 1);
    c[2].text.content = strdup("Up/Down:move  Enter:select  Esc:cancel"); c[2].text.align = ALIGN_CENTER; c[2].text.style = textstyle_muted();
    out->type = RNODE_CONTAINER; out->rect = rect_new(box_x, box_y, box_w, box_h);
    out->container.border = BORDER_SINGLE; out->container.padding = edgeinsets_zero();
    out->container.children = c; out->container.child_count = 3;
}

static EventResult prof_handle(Widget *self, Event *ev, Backend *backend) {
    ProfData *d = (ProfData *)(self + 1);
    if (ev->type != EVENT_KEY) return event_result_unhandled();
    switch (ev->code) {
        case KEY_ESC: return event_result_response((WidgetResponse){ .result = NULL, .cancelled = true, .error = NULL });
        case KEY_UP: if (d->selected > 0) d->selected--; d->dirty = true; return event_result_handled();
        case KEY_DOWN: if (d->selected + 1 < d->count) d->selected++; d->dirty = true; return event_result_handled();
        case KEY_ENTER: return event_result_response((WidgetResponse){ .result = cJSON_CreateString(d->choices[d->selected]), .cancelled = false, .error = NULL });
        default: return event_result_unhandled();
    }
}

static bool prof_dirty(Widget *w) { return ((ProfData *)(w + 1))->dirty; }
static void prof_clear(Widget *w) { ((ProfData *)(w + 1))->dirty = false; }
static void prof_destroy(Widget *w) { ProfData *d = (ProfData *)(w + 1); free(d->title); for (int i = 0; i < d->count; i++) free(d->choices[i]); free(d->choices); }

Widget *profile_factory(const WidgetRequest *req) {
    Widget *w = calloc(1, sizeof(Widget) + sizeof(ProfData));
    w->vtable.render = prof_render; w->vtable.handle_event = prof_handle;
    w->vtable.is_dirty = prof_dirty; w->vtable.clear_dirty = prof_clear; w->vtable.destroy = prof_destroy;
    ProfData *d = (ProfData *)(w + 1);
    d->title = strdup(cJSON_GetObjectItem(req->params, "title")->valuestring ?: "Profile");
    cJSON *ch = cJSON_GetObjectItem(req->params, "choices");
    d->count = ch ? cJSON_GetArraySize(ch) : 0;
    d->choices = malloc(d->count * sizeof(char *));
    for (int i = 0; i < d->count; i++) d->choices[i] = strdup(cJSON_GetArrayItem(ch, i)->valuestring);
    d->selected = 0; d->dirty = true;
    return w;
}