#include <stdio.h>
#include "tabs.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *title;
    char **tab_labels;
    int tab_count;
    Widget **children;
    int child_count;
    int active;
    bool dirty;
} TabsData;

static void tabs_render(Widget *self, Rect area, RenderTree *out) {
    TabsData *d = (TabsData *)(self + 1);
    memset(out, 0, sizeof(*out));
    out->type = RNODE_TABS;
    out->rect = rect_new(0, 0, area.w, area.h);
    out->tabs.tab_labels = malloc(d->tab_count * sizeof(char *));
    out->tabs.tab_count = d->tab_count;
    for (int i = 0; i < d->tab_count; i++) out->tabs.tab_labels[i] = strdup(d->tab_labels[i]);
    out->tabs.active = d->active;
    if (d->active < d->child_count) {
        out->tabs.child = malloc(sizeof(RenderTree));
        d->children[d->active]->vtable.render(d->children[d->active], area, out->tabs.child);
    } else {
        out->tabs.child = malloc(sizeof(RenderTree));
        memset(out->tabs.child, 0, sizeof(RenderTree));
    }
}

static EventResult tabs_handle_event(Widget *self, Event *ev, Backend *backend) {
    TabsData *d = (TabsData *)(self + 1);
    if (ev->type == EVENT_KEY) {
        if (ev->code == KEY_LEFT && d->active > 0) { d->active--; d->dirty = true; return event_result_handled(); }
        if (ev->code == KEY_RIGHT && d->active + 1 < d->tab_count) { d->active++; d->dirty = true; return event_result_handled(); }
    }
    if (d->active < d->child_count)
        return d->children[d->active]->vtable.handle_event(d->children[d->active], ev, backend);
    return event_result_unhandled();
}

static bool tabs_is_dirty(Widget *self) { return ((TabsData *)(self + 1))->dirty; }
static void tabs_clear_dirty(Widget *self) { ((TabsData *)(self + 1))->dirty = false; }
static void tabs_destroy(Widget *self) {
    TabsData *d = (TabsData *)(self + 1);
    free(d->title);
    for (int i = 0; i < d->tab_count; i++) free(d->tab_labels[i]);
    free(d->tab_labels);
    for (int i = 0; i < d->child_count; i++) widget_destroy(d->children[i]);
    free(d->children);
}

Widget *tabs_widget_new(const char *title, char **tab_labels, int tab_count, Widget **children, int child_count) {
    Widget *w = calloc(1, sizeof(Widget) + sizeof(TabsData));
    w->vtable.render = tabs_render;
    w->vtable.handle_event = tabs_handle_event;
    w->vtable.is_dirty = tabs_is_dirty;
    w->vtable.clear_dirty = tabs_clear_dirty;
    w->vtable.destroy = tabs_destroy;
    TabsData *d = (TabsData *)(w + 1);
    d->title = strdup(title);
    d->tab_labels = malloc(tab_count * sizeof(char *));
    d->tab_count = tab_count;
    for (int i = 0; i < tab_count; i++) d->tab_labels[i] = strdup(tab_labels[i]);
    d->children = malloc(child_count * sizeof(Widget *));
    d->child_count = child_count;
    for (int i = 0; i < child_count; i++) d->children[i] = children[i];
    d->active = 0;
    d->dirty = true;
    return w;
}