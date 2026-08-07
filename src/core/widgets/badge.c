#include "badge.h"
#include "core/widget_base.h"
#include <stdlib.h>
#include <string.h>

typedef struct { WidgetBase base; char *text; } BadgeData;

static void badge_render(Widget *self, RenderTree *out) {
    BadgeData *d = (BadgeData *)(self + 1);
    WidgetBase *base = (WidgetBase *)(self + 1);
    Rect area = base->render_area;
    memset(out, 0, sizeof(*out));
    out->type = RNODE_BADGE;
    int is_gui = (area.w > 200);
    int ch = is_gui ? 34 : 1;
    int text_w = is_gui ? (int)strlen(d->text) * 14 + 24 : (int)strlen(d->text) + 4;
    out->rect = rect_new(0, 0, text_w, ch);
    out->u.badge.text = d->text;
    out->style_class = "badge";
    out->accessible.role = "label";
    out->accessible.label = d->text;
}

static EventResult badge_handle_event(Widget *self, Event *ev, Backend *backend) {
    (void)self;
    (void)backend;
    if (ev->type == EVENT_KEY)
        return event_result_response((WidgetResponse){ .result = NULL, .cancelled = false });
    return event_result_unhandled();
}

static void badge_destroy(Widget *self) {
    free(((BadgeData *)(self + 1))->text);
}

Widget *badge_widget_new(const char *text) {
    Widget *w = calloc(1, sizeof(Widget) + sizeof(BadgeData));
    BadgeData *d = (BadgeData *)(w + 1);
    d->base.dirty = true;
    d->base.tab_index = -1;
    d->text = strdup(text);
    w->vtable.render = badge_render;
    w->vtable.handle_event = badge_handle_event;
    w->vtable.is_dirty = widget_base_is_dirty;
    w->vtable.clear_dirty = widget_base_clear_dirty;
    w->vtable.destroy = badge_destroy;
    return w;
}