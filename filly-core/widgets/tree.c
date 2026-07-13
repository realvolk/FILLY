#include <stdio.h>
#include "tree.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *title;
    TreeNode *nodes;
    int node_count;
    int *selected_path;
    int path_len;
    int cursor;
    int *flat_indices;
    int flat_count;
    bool dirty;
} TreeData;

static void flatten(TreeNode **nodes, int count, int *path, int *out_indices, int *out_count, int *cursor_ptr) {
    for (int i = 0; i < count; i++) {
        out_indices[(*out_count)++] = *cursor_ptr;
        (*cursor_ptr)++;
        if (nodes[i]->expanded && nodes[i]->child_count > 0)
            flatten(nodes[i]->children, nodes[i]->child_count, path, out_indices, out_count, cursor_ptr);
    }
}

static void tree_render(Widget *self, Rect area, RenderTree *out) {
    TreeData *d = (TreeData *)(self + 1);
    int total_nodes = 0;
    for (int i = 0; i < d->node_count; i++) total_nodes += d->nodes[i].child_count + 1;
    d->flat_indices = realloc(d->flat_indices, total_nodes * sizeof(int));
    d->flat_count = 0;
    int cursor = 0;
    for (int i = 0; i < d->node_count; i++) {
        d->flat_indices[d->flat_count++] = i;
        if (d->nodes[i].expanded && d->nodes[i].child_count > 0) {
            for (int j = 0; j < d->nodes[i].child_count; j++) {
                d->flat_indices[d->flat_count++] = d->node_count + j;
            }
        }
    }

    memset(out, 0, sizeof(*out));
    int box_w = (int)(area.w * 0.7f);
    if (box_w > area.w - 2) box_w = area.w - 2;
    int box_h = (int)(area.h * 0.8f);
    if (box_h > area.h - 2) box_h = area.h - 2;
    int box_x = (area.w - box_w) / 2, box_y = (area.h - box_h) / 2;

    RenderTree *children = calloc(3 + 1, sizeof(RenderTree));
    children[0].type = RNODE_TEXT;
    children[0].rect = rect_new(1, 0, box_w - 2, 1);
    children[0].text.content = strdup(d->title);
    children[0].text.align = ALIGN_CENTER;
    children[0].text.style = textstyle_selected();

    children[1].type = RNODE_LIST;
    children[1].rect = rect_new(1, 1, box_w - 2, box_h - 3);
    children[1].list.item_count = d->flat_count;
    children[1].list.items = malloc(d->flat_count * sizeof(ListItem));
    for (int i = 0; i < d->flat_count; i++) {
        int idx = d->flat_indices[i];
        const char *label = idx < d->node_count ? d->nodes[idx].label : "?";
        children[1].list.items[i] = listitem_new(label);
    }
    children[1].list.selected = d->cursor;
    children[1].list.highlight = textstyle_selected();

    children[2].type = RNODE_TEXT;
    children[2].rect = rect_new(1, box_h - 2, box_w - 2, 1);
    children[2].text.content = strdup("Up/Down:move  Enter/Space:expand  Esc:cancel");
    children[2].text.align = ALIGN_CENTER;
    children[2].text.style = textstyle_muted();

    out->type = RNODE_CONTAINER;
    out->rect = rect_new(box_x, box_y, box_w, box_h);
    out->container.border = BORDER_SINGLE;
    out->container.padding = edgeinsets_zero();
    out->container.children = children;
    out->container.child_count = 3;
}

static EventResult tree_handle_event(Widget *self, Event *ev, Backend *backend) {
    TreeData *d = (TreeData *)(self + 1);
    if (ev->type != EVENT_KEY) return event_result_unhandled();
    switch (ev->code) {
        case KEY_ESC:
            return event_result_response((WidgetResponse){ .result = NULL, .cancelled = true, .error = NULL });
        case KEY_UP: if (d->cursor > 0) d->cursor--; d->dirty = true; return event_result_handled();
        case KEY_DOWN: if (d->cursor + 1 < d->flat_count) d->cursor++; d->dirty = true; return event_result_handled();
        case KEY_ENTER:
        case KEY_CHAR:
            if (ev->code == KEY_CHAR && ev->ch != ' ') return event_result_unhandled();
            if (d->cursor < d->flat_count) {
                int idx = d->flat_indices[d->cursor];
                if (idx < d->node_count && d->nodes[idx].child_count > 0) {
                    d->nodes[idx].expanded = !d->nodes[idx].expanded;
                    d->dirty = true;
                }
            }
            return event_result_handled();
        default: return event_result_unhandled();
    }
}

static bool tree_is_dirty(Widget *self) { return ((TreeData *)(self + 1))->dirty; }
static void tree_clear_dirty(Widget *self) { ((TreeData *)(self + 1))->dirty = false; }
static void tree_destroy(Widget *self) {
    TreeData *d = (TreeData *)(self + 1);
    free(d->title); free(d->flat_indices);
}

Widget *tree_widget_new(const char *title, TreeNode *nodes, int node_count) {
    Widget *w = calloc(1, sizeof(Widget) + sizeof(TreeData));
    w->vtable.render = tree_render;
    w->vtable.handle_event = tree_handle_event;
    w->vtable.is_dirty = tree_is_dirty;
    w->vtable.clear_dirty = tree_clear_dirty;
    w->vtable.destroy = tree_destroy;
    TreeData *d = (TreeData *)(w + 1);
    d->title = strdup(title);
    d->nodes = nodes;
    d->node_count = node_count;
    d->cursor = 0;
    d->flat_indices = NULL;
    d->flat_count = 0;
    d->dirty = true;
    return w;
}