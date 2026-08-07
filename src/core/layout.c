#include "render.h"
#include <string.h>
#include <stdlib.h>

static int cmp_z_index(const void *a, const void *b) {
    RenderTree *ca = *(RenderTree **)a;
    RenderTree *cb = *(RenderTree **)b;
    return ca->z_index - cb->z_index;
}

static void compute_anchor(RenderTree *node, int surface_w, int surface_h) {
    int x, y;
    switch (node->anchor) {
        case ANCHOR_TOP_LEFT:      x = 0;                    y = 0;                    break;
        case ANCHOR_TOP_CENTER:    x = (surface_w - node->rect.w) / 2; y = 0;           break;
        case ANCHOR_TOP_RIGHT:     x = surface_w - node->rect.w; y = 0;                 break;
        case ANCHOR_CENTER_LEFT:   x = 0;                    y = (surface_h - node->rect.h) / 2; break;
        case ANCHOR_CENTER:        x = (surface_w - node->rect.w) / 2; y = (surface_h - node->rect.h) / 2; break;
        case ANCHOR_CENTER_RIGHT:  x = surface_w - node->rect.w; y = (surface_h - node->rect.h) / 2; break;
        case ANCHOR_BOTTOM_LEFT:   x = 0;                    y = surface_h - node->rect.h; break;
        case ANCHOR_BOTTOM_CENTER: x = (surface_w - node->rect.w) / 2; y = surface_h - node->rect.h; break;
        case ANCHOR_BOTTOM_RIGHT:  x = surface_w - node->rect.w; y = surface_h - node->rect.h; break;
        default: return;
    }
    node->rect.x = x + node->anchor_dx;
    node->rect.y = y + node->anchor_dy;
}

static void clamp_overflow(RenderTree *node, int parent_x, int parent_y, int parent_w, int parent_h) {
    if (node->overflow == OVERFLOW_VISIBLE) return;
    if (node->overflow == OVERFLOW_CLIP) {
        if (node->rect.x < parent_x) node->rect.x = parent_x;
        if (node->rect.y < parent_y) node->rect.y = parent_y;
        if (node->rect.x + node->rect.w > parent_x + parent_w) node->rect.w = parent_x + parent_w - node->rect.x;
        if (node->rect.y + node->rect.h > parent_y + parent_h) node->rect.h = parent_y + parent_h - node->rect.y;
    }
}

static void apply_min_max(RenderTree *node) {
    WidgetStyle *ws = &node->resolved_style;
    if (ws->min_width > 0 && node->rect.w < ws->min_width) node->rect.w = ws->min_width;
    if (ws->min_height > 0 && node->rect.h < ws->min_height) node->rect.h = ws->min_height;
    if (ws->max_width > 0 && node->rect.w > ws->max_width) node->rect.w = ws->max_width;
    if (ws->max_height > 0 && node->rect.h > ws->max_height) node->rect.h = ws->max_height;
}

static int measure_text_width(const char *text, int font_size, bool is_pixel) {
    if (!text) return 0;
    if (is_pixel) return (int)strlen(text) * font_size / 2;
    return (int)strlen(text);
}

static int measure_text_height(const char *text, int font_size, int max_w, bool is_pixel) {
    if (!text) return is_pixel ? 30 : 1;
    int row_h = is_pixel ? font_size + 8 : 1;
    if (max_w <= 0) return row_h;
    int char_w = is_pixel ? font_size / 2 : 1;
    if (char_w <= 0) char_w = 1;
    int lines = 1;
    int col = 0;
    for (const char *p = text; *p; p++) {
        if (*p == '\n') { lines++; col = 0; }
        else { col++; if (col >= max_w / char_w) { lines++; col = 0; } }
    }
    return lines * row_h;
}

static void layout_node(RenderTree *node, int off_x, int off_y, int max_w, int max_h, bool is_pixel);

static void layout_flex(RenderTree *node, int off_x, int off_y, int max_w, int max_h, bool is_pixel) {
    if (!node->u.flex.children || node->u.flex.child_count == 0) return;
    bool is_row = !node->u.flex.direction || strcmp(node->u.flex.direction, "column") != 0;
    bool wrap = node->u.flex.wrap && strcmp(node->u.flex.wrap, "wrap") == 0;
    int row_h = is_pixel ? 30 : 1;
    int avail = is_row ? max_w : max_h;
    int total_grow = 0;
    int fixed_size = 0;

    for (int i = 0; i < node->u.flex.child_count; i++) {
        RenderTree *child = &node->u.flex.children[i];
        int child_size = 0;
        if (child->type == RNODE_TEXT && child->u.text.content)
            child_size = measure_text_width(child->u.text.content, child->resolved_style.font_size, is_pixel);
        else if (child->has_target_rect)
            child_size = is_row ? child->target_rect.w : child->target_rect.h;
        else if (child->rect.w > 0)
            child_size = is_row ? child->rect.w : child->rect.h;
        if (child_size > 0) {
            fixed_size += child_size;
        } else {
            total_grow++;
        }
    }

    int gap = is_pixel ? 8 : 1;
    int remaining = avail - fixed_size - gap * (node->u.flex.child_count - 1);
    int grow_unit = total_grow > 0 ? remaining / total_grow : 0;
    int pos = is_row ? off_x : off_y;
    int line_start = 0;
    int line_pos = pos;
    int line_h = 0;

    for (int i = 0; i < node->u.flex.child_count; i++) {
        RenderTree *child = &node->u.flex.children[i];
        int child_w, child_h;

        if (child->has_target_rect) {
            child_w = child->target_rect.w;
            child_h = child->target_rect.h;
        } else {
            child_w = child->rect.w;
            child_h = child->rect.h;
        }

        if (child_w <= 0) {
            child_w = grow_unit > 0 ? grow_unit : avail / node->u.flex.child_count;
        }
        if (child_h <= 0) child_h = row_h;

        if (child->resolved_style.min_width > 0 && child_w < child->resolved_style.min_width)
            child_w = child->resolved_style.min_width;
        if (child->resolved_style.min_height > 0 && child_h < child->resolved_style.min_height)
            child_h = child->resolved_style.min_height;

        if (wrap && is_row && line_pos + child_w > off_x + max_w && line_pos > off_x) {
            pos = off_y + line_h + gap;
            line_pos = off_x;
            line_start = i;
        }

        if (is_row) {
            child->rect.x = line_pos;
            child->rect.y = pos;
            child->rect.w = child_w;
            child->rect.h = child_h;
        } else {
            child->rect.x = off_x;
            child->rect.y = line_pos;
            child->rect.w = max_w;
            child->rect.h = child_h;
        }

        if (child_h > line_h) line_h = child_h;
        line_pos += (is_row ? child_w : child_h) + gap;
    }

    for (int i = 0; i < node->u.flex.child_count; i++)
        apply_min_max(&node->u.flex.children[i]);

    if (is_row) {
        node->rect.w = max_w;
        node->rect.h = off_y + line_h + gap - node->rect.y;
    } else {
        node->rect.w = max_w;
        node->rect.h = max_h;
    }
}

static void layout_grid(RenderTree *node, int off_x, int off_y, int max_w, int max_h, bool is_pixel) {
    if (!node->u.grid.children || node->u.grid.child_count == 0) return;
    int cols = node->u.grid.columns > 0 ? node->u.grid.columns : 1;
    int gap = is_pixel ? 8 : 1;
    int cell_w = (max_w - gap * (cols - 1)) / cols;
    int row_h = is_pixel ? 30 : 1;

    for (int i = 0; i < node->u.grid.child_count; i++) {
        RenderTree *child = &node->u.grid.children[i];
        int col = i % cols;
        int row = i / cols;
        child->rect.x = off_x + col * (cell_w + gap);
        child->rect.y = off_y + row * (row_h + gap);
        child->rect.w = cell_w;
        child->rect.h = row_h;
        apply_min_max(child);
    }

    int rows = (node->u.grid.child_count + cols - 1) / cols;
    node->rect.w = max_w;
    node->rect.h = rows * (row_h + gap) - gap;
}

static void layout_node(RenderTree *node, int off_x, int off_y, int max_w, int max_h, bool is_pixel) {
    if (!node || max_w <= 0 || max_h <= 0) return;

    if (node->has_target_rect) {
        node->rect = node->target_rect;
        node->has_target_rect = false;
    }

    if (node->anchor != ANCHOR_ABSOLUTE && node->anchor != ANCHOR_RELATIVE) {
        compute_anchor(node, max_w, max_h);
    }

    apply_min_max(node);
    clamp_overflow(node, off_x, off_y, max_w, max_h);

    int content_x = off_x + node->rect.x;
    int content_y = off_y + node->rect.y;
    int content_w = node->rect.w;
    int content_h = node->rect.h;

    if (node->type == RNODE_CONTAINER && node->u.container.children) {
        RenderTree **sorted = malloc(node->u.container.child_count * sizeof(RenderTree *));
        for (int i = 0; i < node->u.container.child_count; i++)
            sorted[i] = &node->u.container.children[i];
        qsort(sorted, node->u.container.child_count, sizeof(RenderTree *), cmp_z_index);

        int row_h = is_pixel ? 30 : 1;
        int cur_y = content_y;
        for (int i = 0; i < node->u.container.child_count; i++) {
            RenderTree *child = sorted[i];
            if (child->anchor == ANCHOR_ABSOLUTE) {
                layout_node(child, content_x, content_y, content_w, content_h, is_pixel);
            } else if (child->anchor == ANCHOR_RELATIVE) {
                if (child->relative_to) {
                    for (int j = 0; j < node->u.container.child_count; j++) {
                        RenderTree *rel = &node->u.container.children[j];
                        if (rel->style_class && strcmp(rel->style_class, child->relative_to) == 0) {
                            child->rect.x = rel->rect.x + child->anchor_dx;
                            child->rect.y = rel->rect.y + child->anchor_dy;
                            break;
                        }
                    }
                }
                child->rect.x += child->anchor_dx;
                child->rect.y += child->anchor_dy;
                layout_node(child, content_x, content_y, content_w, content_h, is_pixel);
            } else {
                child->rect.x = content_x;
                child->rect.y = cur_y;
                child->rect.w = content_w;
                child->rect.h = row_h;
                if (child->type == RNODE_TEXT && child->u.text.content) {
                    child->rect.h = measure_text_height(child->u.text.content, child->resolved_style.font_size, content_w, is_pixel);
                } else if (child->type == RNODE_LIST && child->u.list.item_count > 0) {
                    child->rect.h = row_h * child->u.list.item_count;
                } else if (child->type == RNODE_TABLE) {
                    child->rect.h = row_h * (child->u.table.row_count + 1);
                } else if (child->type == RNODE_TREE) {
                    child->rect.h = row_h * child->u.tree.node_count;
                } else if (child->type == RNODE_CALENDAR) {
                    child->rect.h = row_h * 10;
                } else if (child->type == RNODE_GAUGE) {
                    child->rect.h = row_h * 2;
                } else if (child->type == RNODE_FORM) {
                    child->rect.h = row_h * (child->u.form.field_count + 1);
                }
                layout_node(child, content_x, cur_y, content_w, child->rect.h, is_pixel);
                cur_y += child->rect.h;
            }
        }
        free(sorted);
    }

    if (node->type == RNODE_FLEX) {
        layout_flex(node, content_x, content_y, content_w, content_h, is_pixel);
    }

    if (node->type == RNODE_GRID) {
        layout_grid(node, content_x, content_y, content_w, content_h, is_pixel);
    }

    if (node->type == RNODE_TABS && node->u.tabs.child) {
        int tab_h = is_pixel ? 30 : 1;
        int child_y = content_y + tab_h;
        layout_node(node->u.tabs.child, content_x, child_y, content_w, content_h - tab_h, is_pixel);
    }

    if (node->type == RNODE_SPLIT_PANES) {
        int sp = node->u.split_panes.split_position > 0 ? node->u.split_panes.split_position :
                  (node->u.split_panes.orientation == ORIENT_HORIZONTAL ? content_w / 2 : content_h / 2);
        if (node->u.split_panes.orientation == ORIENT_HORIZONTAL) {
            if (node->u.split_panes.first) layout_node(node->u.split_panes.first, content_x, content_y, sp, content_h, is_pixel);
            if (node->u.split_panes.second) layout_node(node->u.split_panes.second, content_x + sp + 1, content_y, content_w - sp - 1, content_h, is_pixel);
        } else {
            if (node->u.split_panes.first) layout_node(node->u.split_panes.first, content_x, content_y, content_w, sp, is_pixel);
            if (node->u.split_panes.second) layout_node(node->u.split_panes.second, content_x, content_y + sp + 1, content_w, content_h - sp - 1, is_pixel);
        }
    }
}

void layout_tree(RenderTree *tree, int surface_w, int surface_h, bool is_pixel) {
    if (!tree || surface_w <= 0 || surface_h <= 0) return;

    tree->rect.x = 0;
    tree->rect.y = 0;
    tree->rect.w = surface_w;
    tree->rect.h = surface_h;

    layout_node(tree, 0, 0, surface_w, surface_h, is_pixel);
}