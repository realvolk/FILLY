#include "render.h"
#include "theme.h"
#include "arena.h"
#include <stdlib.h>
#include <string.h>

Rect rect_new(int x, int y, int w, int h) {
    Rect r = { .x = x, .y = y, .w = w, .h = h };
    return r;
}

EdgeInsets edgeinsets_zero(void) {
    EdgeInsets e = { .top = 0, .bottom = 0, .left = 0, .right = 0 };
    return e;
}

ListItem listitem_new(const char *label) {
    (void)label;
    ListItem li = { .label = NULL, .meta = NULL };
    return li;
}

WidgetStyle widgetstyle_default(void) {
    WidgetStyle s;
    memset(&s, 0, sizeof(s));
    s.fg_color = rgba(0xd0, 0xd0, 0xd0, 255);
    s.bg_color = rgba(0x1a, 0x1a, 0x2e, 255);
    s.border_color = rgba(0x2a, 0x2a, 0x4a, 255);
    s.accent_color = rgba(0xe9, 0x45, 0x60, 255);
    s.border_width = 1;
    s.border_radius = 4;
    s.padding[0] = 8; s.padding[1] = 12; s.padding[2] = 8; s.padding[3] = 12;
    s.font_size = 14;
    s.font_weight = 400;
    s.font_family = NULL;  /* will be filled by theme */
    return s;
}

void resolve_node_styles(RenderTree *node, Theme *theme) {
    if (!node) return;
    WidgetStyle *ws = &node->resolved_style;
    if (node->style_class && node->style_class[0]) {
        const char *st = node->state ? node->state : "normal";
        WidgetStyle resolved = theme_resolve(theme, node->style_class, NULL, st);
        if (resolved.font_size != 0) {
            node->prev_resolved_style = *ws;
            *ws = resolved;
        }
    }
    if (ws->font_size == 0) ws->font_size = 14;
    if (ws->fg_color == 0) ws->fg_color = rgba(208, 208, 208, 255);
    if (ws->bg_color == 0) ws->bg_color = rgba(26, 26, 46, 255);
    if (ws->accent_color == 0) ws->accent_color = rgba(233, 69, 96, 255);
    if (ws->border_color == 0) ws->border_color = rgba(42, 42, 74, 255);
    if (ws->border_width == 0) ws->border_width = 1;

    if (node->type == RNODE_CONTAINER) {
        for (int i = 0; i < node->container.child_count; i++)
            resolve_node_styles(&node->container.children[i], theme);
    }
    if (node->type == RNODE_TABS && node->tabs.child)
        resolve_node_styles(node->tabs.child, theme);
    if (node->type == RNODE_SPLIT_PANES) {
        if (node->split_panes.first) resolve_node_styles(node->split_panes.first, theme);
        if (node->split_panes.second) resolve_node_styles(node->split_panes.second, theme);
    }
}

void render_tree_mark_dirty(RenderTree *tree) {
    if (!tree) return;
    tree->dirty = true;
    if (tree->type == RNODE_CONTAINER) {
        for (int i = 0; i < tree->container.child_count; i++)
            render_tree_mark_dirty(&tree->container.children[i]);
    }
}

void render_tree_free(RenderTree *tree) {
    (void)tree;
    /* arena-backed: nothing to free. reset arena in session_run after each frame. */
}