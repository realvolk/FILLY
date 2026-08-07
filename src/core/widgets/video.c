#include "video.h"
#include "core/widget_base.h"
#include "core/session.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    WidgetBase base;
    char *source;
    bool loop;
} VideoData;

extern Arena *g_session_arena;

static void video_render(Widget *self, RenderTree *out) {
    VideoData *d = (VideoData *)(self + 1);
    WidgetBase *base = (WidgetBase *)(self + 1);
    Rect area = base->render_area;
    memset(out, 0, sizeof(*out));
    out->type = RNODE_VIDEO;
    out->u.video.source = arena_strdup(g_session_arena, d->source);
    out->u.video.loop = d->loop;
    out->rect = rect_new(0, 0, area.w, area.h);
    out->style_class = "video";
    out->accessible.role = "video";
    out->accessible.label = d->source ? d->source : "Video";
    out->tab_index = base->tab_index >= 0 ? base->tab_index : -1;
}

static EventResult video_handle_event(Widget *self, Event *ev, Backend *backend) {
    (void)self; (void)backend;
    if (ev->type == EVENT_KEY)
        return event_result_response((WidgetResponse){ .result = NULL, .cancelled = false });
    return event_result_unhandled();
}

static void video_destroy(Widget *self) {
    VideoData *d = (VideoData *)(self + 1);
    free(d->source);
}

Widget *video_widget_new(const char *source, bool loop) {
    Widget *w = calloc(1, sizeof(Widget) + sizeof(VideoData));
    VideoData *d = (VideoData *)(w + 1);
    d->base.dirty = true;
    d->base.tab_index = -1;
    d->source = source ? strdup(source) : strdup("");
    d->loop = loop;
    w->vtable.render = video_render;
    w->vtable.handle_event = video_handle_event;
    w->vtable.is_dirty = widget_base_is_dirty;
    w->vtable.clear_dirty = widget_base_clear_dirty;
    w->vtable.destroy = video_destroy;
    return w;
}