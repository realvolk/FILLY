#include "tabs.h"
#include "core/widget_base.h"
#include "core/session.h"
#include <stdlib.h>
#include <string.h>

typedef struct { WidgetBase base; char *title, **tab_labels; int tab_count, active; Widget **children; int child_count; } TabsData;
extern Arena *g_session_arena;

static void tabs_render(Widget *self, Rect area, RenderTree *out) {
    TabsData *d = (TabsData *)(self + 1);
    memset(out, 0, sizeof(*out)); out->type = RNODE_TABS; out->rect = area;
    out->tabs.tab_labels = arena_alloc(g_session_arena, d->tab_count * sizeof(char *)); out->tabs.tab_count = d->tab_count;
    for (int i=0;i<d->tab_count;i++) out->tabs.tab_labels[i] = arena_strdup(g_session_arena, d->tab_labels[i]);
    out->tabs.active = d->active; out->style_class = "tabs";
    if (d->active < d->child_count) { out->tabs.child = arena_alloc(g_session_arena, sizeof(RenderTree)); d->children[d->active]->vtable.render(d->children[d->active], area, out->tabs.child); }
}

static EventResult tabs_handle_event(Widget *self, Event *ev, Backend *backend) {
    TabsData *d = (TabsData *)(self + 1);
    if (ev->type == EVENT_KEY) { if (ev->code == KEY_LEFT && d->active > 0) { d->active--; d->base.dirty = true; return event_result_handled(); } if (ev->code == KEY_RIGHT && d->active+1 < d->tab_count) { d->active++; d->base.dirty = true; return event_result_handled(); } }
    if (d->active < d->child_count) return d->children[d->active]->vtable.handle_event(d->children[d->active], ev, backend);
    return event_result_unhandled();
}

static void tabs_destroy(Widget *self) { TabsData *d = (TabsData *)(self + 1); free(d->title); for (int i=0;i<d->tab_count;i++) free(d->tab_labels[i]); free(d->tab_labels); for (int i=0;i<d->child_count;i++) widget_destroy(d->children[i]); free(d->children); }

Widget *tabs_widget_new(const char *title, char **tab_labels, int tab_count, Widget **children, int child_count) {
    Widget *w = calloc(1, sizeof(Widget) + sizeof(TabsData));
    TabsData data = { .title = strdup(title), .tab_count = tab_count, .active = 0, .child_count = child_count };
    data.tab_labels = malloc(tab_count * sizeof(char *)); for (int i=0;i<tab_count;i++) data.tab_labels[i]=strdup(tab_labels[i]);
    data.children = malloc(child_count * sizeof(Widget *)); for (int i=0;i<child_count;i++) data.children[i]=children[i];
    widget_base_init(w, &data, sizeof(TabsData), tabs_render, tabs_handle_event, tabs_destroy);
    return w;
}