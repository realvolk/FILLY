#include "yesno.h"
#include "core/widget_base.h"
#include "core/session.h"
#include <stdlib.h>
#include <string.h>

typedef struct { WidgetBase base; char *title, *message; bool selected_yes; } YesNoData;
extern Arena *g_session_arena;

static void yesno_render(Widget *self, Rect area, RenderTree *out) {
    YesNoData *d = (YesNoData *)(self + 1);
    memset(out, 0, sizeof(*out)); out->style_class = "container";
    int box_w = 50, box_h = 10; if (box_w > area.w - 2) box_w = area.w - 2; if (box_h > area.h - 2) box_h = area.h - 2;
    int child_count = (d->title ? 1 : 0) + (d->message ? 1 : 0) + 3;
    RenderTree *children = arena_alloc(g_session_arena, child_count * sizeof(RenderTree));
    int idx = 0;
    if (d->title && strlen(d->title)) { children[idx].type = RNODE_TEXT; children[idx].rect = rect_new(1, 0, box_w - 2, 1); children[idx].text.content = arena_strdup(g_session_arena, d->title); children[idx].style_class = "text"; children[idx].state = "title"; idx++; }
    if (d->message && strlen(d->message)) { children[idx].type = RNODE_TEXT; children[idx].rect = rect_new(1, 2, box_w - 2, 2); children[idx].text.content = arena_strdup(g_session_arena, d->message); children[idx].style_class = "text"; idx++; }
    children[idx].type = RNODE_TEXT; children[idx].rect = rect_new((box_w - 20) / 2, 5, 8, 1); children[idx].text.content = "[ Yes ]"; children[idx].style_class = "text"; children[idx].state = d->selected_yes ? "selected" : "normal"; idx++;
    children[idx].type = RNODE_TEXT; children[idx].rect = rect_new((box_w - 20) / 2 + 10, 5, 8, 1); children[idx].text.content = "[ No ]"; children[idx].style_class = "text"; children[idx].state = d->selected_yes ? "normal" : "selected"; idx++;
    children[idx].type = RNODE_TEXT; children[idx].rect = rect_new(1, box_h - 2, box_w - 2, 1); children[idx].text.content = "h/l:choose  Enter:confirm  y/n:quick  Esc:cancel"; children[idx].style_class = "text"; children[idx].state = "muted"; idx++;
    out->type = RNODE_CONTAINER; out->rect = rect_new((area.w - box_w) / 2, (area.h - box_h) / 2, box_w, box_h);
    out->container.border = BORDER_SINGLE; out->container.padding = edgeinsets_zero();
    out->container.children = children; out->container.child_count = idx;
}

static EventResult yesno_handle_event(Widget *self, Event *ev, Backend *backend) {
    (void)backend; YesNoData *d = (YesNoData *)(self + 1);
    if (ev->type != EVENT_KEY) return event_result_unhandled();
    switch (ev->code) {
        case KEY_ESC: return event_result_response((WidgetResponse){ .result = NULL, .cancelled = true });
        case KEY_LEFT: case KEY_RIGHT: case KEY_TAB: d->selected_yes = !d->selected_yes; d->base.dirty = true; return event_result_handled();
        case KEY_ENTER: return event_result_response((WidgetResponse){ .result = cJSON_CreateBool(d->selected_yes ? 1 : 0), .cancelled = false });
        case KEY_CHAR: if (ev->ch == 'y' || ev->ch == 'Y') return event_result_response((WidgetResponse){ .result = cJSON_CreateBool(1), .cancelled = false }); if (ev->ch == 'n' || ev->ch == 'N') return event_result_response((WidgetResponse){ .result = cJSON_CreateBool(0), .cancelled = false }); return event_result_unhandled();
        default: return event_result_unhandled();
    }
}

static void yesno_destroy(Widget *self) { YesNoData *d = (YesNoData *)(self + 1); free(d->title); free(d->message); }

Widget *yesno_widget_new(const char *title, const char *message, bool default_yes) {
    Widget *w = calloc(1, sizeof(Widget) + sizeof(YesNoData));
    YesNoData *d = (YesNoData *)(w + 1);
    d->base.dirty = true; d->title = title ? strdup(title) : NULL; d->message = message ? strdup(message) : NULL; d->selected_yes = default_yes;
    w->vtable.render = yesno_render; w->vtable.handle_event = yesno_handle_event;
    w->vtable.is_dirty = widget_base_is_dirty; w->vtable.clear_dirty = widget_base_clear_dirty; w->vtable.destroy = yesno_destroy;
    return w;
}