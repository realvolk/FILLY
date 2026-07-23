#include "summary.h"
#include "core/widget_base.h"
#include "core/session.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct { WidgetBase base; char *title, *text; } SummaryData;
extern Arena *g_session_arena;

static void summary_render(Widget *self, Rect area, RenderTree *out) {
    SummaryData *d = (SummaryData *)(self + 1);
    memset(out, 0, sizeof(*out));
    out->style_class = "container";
    int box_w = (int)(area.w * 0.8f);
    if (box_w > area.w - 2) box_w = area.w - 2;
    int box_h = (int)(area.h * 0.8f);
    if (box_h > area.h - 2) box_h = area.h - 2;
    RenderTree *children = arena_alloc(g_session_arena, 3 * sizeof(RenderTree));
    children[0].type = RNODE_TEXT;
    children[0].rect = rect_new(1, 0, box_w - 2, 1);
    children[0].text.content = arena_strdup(g_session_arena, d->title);
    children[0].style_class = "text";
    children[0].state = "title";
    children[1].type = RNODE_TEXT;
    children[1].rect = rect_new(1, 1, box_w - 2, box_h - 3);
    children[1].text.content = arena_strdup(g_session_arena, d->text);
    children[1].style_class = "text";
    children[2].type = RNODE_TEXT;
    children[2].rect = rect_new(1, box_h - 2, box_w - 2, 1);
    children[2].text.content = "Any key to continue  Esc:cancel";
    children[2].style_class = "text";
    children[2].state = "muted";
    out->type = RNODE_CONTAINER;
    out->rect = rect_new((area.w - box_w) / 2, (area.h - box_h) / 2, box_w, box_h);
    out->container.border = BORDER_SINGLE;
    out->container.padding = edgeinsets_zero();
    out->container.children = children;
    out->container.child_count = 3;
}

static EventResult summary_handle_event(Widget *self, Event *ev, Backend *backend) {
    (void)self;
    (void)backend;
    if (ev->type == EVENT_KEY) {
        if (ev->code == KEY_ESC)
            return event_result_response((WidgetResponse){ .result = NULL, .cancelled = true });
        return event_result_response((WidgetResponse){ .result = NULL, .cancelled = false });
    }
    return event_result_unhandled();
}

static void summary_destroy(Widget *self) {
    SummaryData *d = (SummaryData *)(self + 1);
    free(d->title);
    free(d->text);
}

Widget *summary_widget_new(const char *title, const char *message, const char *file_path) {
    Widget *w = calloc(1, sizeof(Widget) + sizeof(SummaryData));
    SummaryData *d = (SummaryData *)(w + 1);
    d->base.dirty = true;
    d->title = strdup(title);
    if (file_path) {
        FILE *f = fopen(file_path, "r");
        if (f) {
            fseek(f, 0, SEEK_END);
            long sz = ftell(f);
            rewind(f);
            d->text = malloc(sz + 1);
            fread(d->text, 1, sz, f);
            d->text[sz] = '\0';
            fclose(f);
        } else {
            d->text = strdup("[Error reading file]");
        }
    } else {
        d->text = message ? strdup(message) : strdup("");
    }
    w->vtable.render = summary_render;
    w->vtable.handle_event = summary_handle_event;
    w->vtable.is_dirty = widget_base_is_dirty;
    w->vtable.clear_dirty = widget_base_clear_dirty;
    w->vtable.destroy = summary_destroy;
    return w;
}