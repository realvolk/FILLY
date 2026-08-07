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
    ListItem li = { .label = (char *)label, .meta = NULL };
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
    s.border_top_style = BORDER_SOLID;
    s.border_bottom_style = BORDER_SOLID;
    s.border_left_style = BORDER_SOLID;
    s.border_right_style = BORDER_SOLID;
    s.padding[0] = 8; s.padding[1] = 12; s.padding[2] = 8; s.padding[3] = 12;
    s.font_size = 14;
    s.font_weight = 400;
    s.font_family = NULL;
    s.opacity = 1.0f;
    s.scale_x = 1.0f;
    s.scale_y = 1.0f;
    s.rotation = 0.0f;
    s.translate_x = 0.0f;
    s.translate_y = 0.0f;
    s.shadow_count = 0;
    s.gradient.type = GRADIENT_NONE;
    s.backdrop_blur = 0;
    s.letter_spacing = 0.0f;
    s.line_height = 1.2f;
    s.text_transform = 0;
    s.transition_ms = 150;
    s.cursor_style = CURSOR_DEFAULT;
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

    if (node->context) {
        if (node->context->parent_rect.w > 0) {
            if (ws->max_width == 0 || ws->max_width > node->context->parent_rect.w)
                ws->max_width = node->context->parent_rect.w;
        }
        if (node->context->screen_width > 0 && ws->min_width == 0)
            ws->min_width = 1;
    }

    if (node->type == RNODE_CONTAINER) {
        for (int i = 0; i < node->u.container.child_count; i++)
            resolve_node_styles(&node->u.container.children[i], theme);
    }
    if (node->type == RNODE_TABS && node->u.tabs.child)
        resolve_node_styles(node->u.tabs.child, theme);
    if (node->type == RNODE_SPLIT_PANES) {
        if (node->u.split_panes.first) resolve_node_styles(node->u.split_panes.first, theme);
        if (node->u.split_panes.second) resolve_node_styles(node->u.split_panes.second, theme);
        if (node->u.split_panes.third) resolve_node_styles(node->u.split_panes.third, theme);
    }
    if (node->type == RNODE_FLEX && node->u.flex.children) {
        for (int i = 0; i < node->u.flex.child_count; i++)
            resolve_node_styles(&node->u.flex.children[i], theme);
    }
    if (node->type == RNODE_GRID && node->u.grid.children) {
        for (int i = 0; i < node->u.grid.child_count; i++)
            resolve_node_styles(&node->u.grid.children[i], theme);
    }
}

void render_tree_mark_dirty(RenderTree *tree) {
    if (!tree) return;
    tree->dirty = true;
    if (tree->type == RNODE_CONTAINER) {
        for (int i = 0; i < tree->u.container.child_count; i++)
            render_tree_mark_dirty(&tree->u.container.children[i]);
    }
    if (tree->type == RNODE_FLEX && tree->u.flex.children) {
        for (int i = 0; i < tree->u.flex.child_count; i++)
            render_tree_mark_dirty(&tree->u.flex.children[i]);
    }
    if (tree->type == RNODE_GRID && tree->u.grid.children) {
        for (int i = 0; i < tree->u.grid.child_count; i++)
            render_tree_mark_dirty(&tree->u.grid.children[i]);
    }
}

void render_tree_free(RenderTree *tree) {
    (void)tree;
}