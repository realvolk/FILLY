#include "core/widget.h"
#include "core/render.h"
#include "core/widget_base.h"
#include "core/session.h"
#include "protocol/protocol.h"
#include "cJSON.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    WidgetBase base;
    char *title;
    char **choices;
    int count;
    int selected;
} KernelData;

extern Arena *g_session_arena;

static void kern_render(Widget *self, Rect area, RenderTree *out) {
    KernelData *d = (KernelData *)(self + 1);
    memset(out, 0, sizeof(*out));
    out->style_class = "container";
    int box_w = (int)(area.w * 0.6f), box_h = (int)(area.h * 0.7f);
    if (box_w > area.w - 2) box_w = area.w - 2;
    int box_x = (area.w - box_w) / 2, box_y = (area.h - box_h) / 2;
    RenderTree *c = arena_alloc(g_session_arena, 3 * sizeof(RenderTree));
    c[0].type = RNODE_TEXT; c[0].rect = rect_new(1, 0, box_w - 2, 1);
    c[0].text.content = arena_strdup(g_session_arena, d->title);
    c[0].style_class = "text"; c[0].state = "title";
    c[1].type = RNODE_LIST; c[1].rect = rect_new(1, 1, box_w - 2, box_h - 3);
    c[1].list.item_count = d->count; c[1].list.selected = d->selected;
    c[1].list.items = arena_alloc(g_session_arena, d->count * sizeof(ListItem));
    for (int i = 0; i < d->count; i++) c[1].list.items[i].label = arena_strdup(g_session_arena, d->choices[i]);
    c[1].style_class = "list";
    c[2].type = RNODE_TEXT; c[2].rect = rect_new(1, box_h - 2, box_w - 2, 1);
    c[2].text.content = "Up/Down:move  Enter:select  Esc:cancel";
    c[2].style_class = "text"; c[2].state = "muted";
    out->type = RNODE_CONTAINER; out->rect = rect_new(box_x, box_y, box_w, box_h);
    out->container.border = BORDER_SINGLE; out->container.padding = edgeinsets_zero();
    out->container.children = c; out->container.child_count = 3;
}

static EventResult kern_handle(Widget *self, Event *ev, Backend *backend) {
    (void)backend;
    KernelData *d = (KernelData *)(self + 1);
    if (ev->type != EVENT_KEY) return event_result_unhandled();
    switch (ev->code) {
        case KEY_ESC: return event_result_response((WidgetResponse){ .result = NULL, .cancelled = true });
        case KEY_UP: if (d->selected > 0) d->selected--; d->base.dirty = true; return event_result_handled();
        case KEY_DOWN: if (d->selected + 1 < d->count) d->selected++; d->base.dirty = true; return event_result_handled();
        case KEY_ENTER:
            if (d->count > 0 && d->selected < d->count)
                return event_result_response((WidgetResponse){ .result = cJSON_CreateString(d->choices[d->selected]), .cancelled = false });
            return event_result_handled();
        default: return event_result_unhandled();
    }
}

static void kern_destroy(Widget *w) {
    KernelData *d = (KernelData *)(w + 1);
    free(d->title);
    for (int i = 0; i < d->count; i++) free(d->choices[i]);
    free(d->choices);
}

Widget *kernel_factory(const WidgetRequest *req) {
    Widget *w = calloc(1, sizeof(Widget) + sizeof(KernelData));
    KernelData data;
    memset(&data, 0, sizeof(data));
    cJSON *title_j = cJSON_GetObjectItem(req->params, "title");
    data.title = strdup(title_j && title_j->valuestring ? title_j->valuestring : "Kernel");
    cJSON *ch = cJSON_GetObjectItem(req->params, "choices");
    data.count = (ch && ch->type == cJSON_Array) ? cJSON_GetArraySize(ch) : 0;
    data.choices = data.count > 0 ? malloc(data.count * sizeof(char *)) : NULL;
    for (int i = 0; i < data.count; i++) data.choices[i] = strdup(cJSON_GetArrayItem(ch, i)->valuestring);
    data.selected = 0;
    widget_base_init(w, &data, sizeof(KernelData), kern_render, kern_handle, kern_destroy);
    return w;
}