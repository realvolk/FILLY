#include "tree.h"
#include "core/widget_base.h"
#include "core/session.h"
#include <stdlib.h>
#include <string.h>

typedef struct { WidgetBase base; char *title; TreeNode *nodes; int node_count, *flat_indices, flat_count, cursor; } TreeData;
extern Arena *g_session_arena;

static void tree_render(Widget *self, Rect area, RenderTree *out) {
    TreeData *d = (TreeData *)(self + 1);
    d->flat_indices = realloc(d->flat_indices, (d->node_count*10) * sizeof(int)); d->flat_count = 0;
    for (int i = 0; i < d->node_count; i++) { d->flat_indices[d->flat_count++] = i; if (d->nodes[i].expanded && d->nodes[i].child_count > 0) for (int j=0;j<d->nodes[i].child_count;j++) d->flat_indices[d->flat_count++] = d->node_count + j; }
    memset(out, 0, sizeof(*out)); out->style_class = "container";
    int box_w = (int)(area.w * 0.7f); if (box_w > area.w - 2) box_w = area.w - 2;
    int box_h = (int)(area.h * 0.8f); if (box_h > area.h - 2) box_h = area.h - 2;
    RenderTree *children = arena_alloc(g_session_arena, 3 * sizeof(RenderTree));
    children[0].type = RNODE_TEXT; children[0].rect = rect_new(1, 0, box_w - 2, 1);
    children[0].text.content = arena_strdup(g_session_arena, d->title); children[0].style_class = "text"; children[0].state = "title";
    children[1].type = RNODE_TREE; children[1].rect = rect_new(1, 1, box_w - 2, box_h - 3);
    children[1].tree.nodes = d->nodes; children[1].tree.node_count = d->node_count; children[1].style_class = "tree";
    children[2].type = RNODE_TEXT; children[2].rect = rect_new(1, box_h - 2, box_w - 2, 1);
    children[2].text.content = "Up/Down:move  Enter/Space:expand  Esc:dismiss"; children[2].style_class = "text"; children[2].state = "muted";
    out->type = RNODE_CONTAINER; out->rect = rect_new((area.w - box_w) / 2, (area.h - box_h) / 2, box_w, box_h);
    out->container.border = BORDER_SINGLE; out->container.padding = edgeinsets_zero();
    out->container.children = children; out->container.child_count = 3;
}

static EventResult tree_handle_event(Widget *self, Event *ev, Backend *backend) {
    (void)backend; TreeData *d = (TreeData *)(self + 1);
    if (ev->type != EVENT_KEY) return event_result_unhandled();
    switch (ev->code) {
        case KEY_ESC: return event_result_response((WidgetResponse){ .result = NULL, .cancelled = false });
        case KEY_UP: if (d->cursor > 0) d->cursor--; d->base.dirty = true; return event_result_handled();
        case KEY_DOWN: if (d->cursor + 1 < d->flat_count) d->cursor++; d->base.dirty = true; return event_result_handled();
        case KEY_ENTER: case KEY_CHAR: if (ev->code == KEY_CHAR && ev->ch != ' ') return event_result_unhandled(); if (d->cursor < d->flat_count) { int idx = d->flat_indices[d->cursor]; if (idx < d->node_count && d->nodes[idx].child_count > 0) { d->nodes[idx].expanded = !d->nodes[idx].expanded; d->base.dirty = true; } } return event_result_handled();
        default: return event_result_unhandled();
    }
}

static void tree_destroy(Widget *self) { TreeData *d = (TreeData *)(self + 1); free(d->title); free(d->flat_indices); }

Widget *tree_widget_new(const char *title, TreeNode *nodes, int node_count) {
    Widget *w = calloc(1, sizeof(Widget) + sizeof(TreeData));
    TreeData data = { .title = strdup(title), .nodes = nodes, .node_count = node_count, .flat_indices = NULL, .flat_count = 0, .cursor = 0 };
    widget_base_init(w, &data, sizeof(TreeData), tree_render, tree_handle_event, tree_destroy);
    return w;
}