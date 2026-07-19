#include "render.h"
#include <stdlib.h>
#include <string.h>

TextStyle textstyle_normal(void) {
    TextStyle s = { .fg = "7", .bg = NULL, .bold = true, .italic = false, .underline = false };
    return s;
}

TextStyle textstyle_accent(void) {
    TextStyle s = { .fg = "2", .bg = NULL, .bold = true, .italic = false, .underline = false };
    return s;
}

TextStyle textstyle_muted(void) {
    TextStyle s = { .fg = "8", .bg = NULL, .bold = true, .italic = false, .underline = false };
    return s;
}

TextStyle textstyle_selected(void) {
    TextStyle s = { .fg = "2", .bg = "0", .bold = true, .italic = false, .underline = false };
    return s;
}

Rect rect_new(int x, int y, int w, int h) {
    Rect r = { .x = x, .y = y, .w = w, .h = h };
    return r;
}

EdgeInsets edgeinsets_zero(void) {
    EdgeInsets e = { .top = 0, .bottom = 0, .left = 0, .right = 0 };
    return e;
}

ListItem listitem_new(const char *label) {
    ListItem li = { .label = strdup(label), .meta = NULL };
    return li;
}

void render_tree_free(RenderTree *tree) {
    if (!tree) return;
    free(tree->accessible.role);
    free(tree->accessible.label);
    switch (tree->type) {
    case RNODE_CONTAINER:
        for (int i = 0; i < tree->container.child_count; i++)
            render_tree_free(&tree->container.children[i]);
        free(tree->container.children);
        free(tree->container.bg);
        break;
    case RNODE_TEXT:
        free(tree->text.content);
        if (tree->text.style.fg && tree->text.style.fg != textstyle_normal().fg &&
            tree->text.style.fg != textstyle_accent().fg &&
            tree->text.style.fg != textstyle_muted().fg &&
            tree->text.style.fg != textstyle_selected().fg)
            free(tree->text.style.fg);
        if (tree->text.style.bg && tree->text.style.bg != textstyle_selected().bg)
            free(tree->text.style.bg);
        break;
    case RNODE_LIST:
        for (int i = 0; i < tree->list.item_count; i++)
            free(tree->list.items[i].label);
        free(tree->list.items);
        break;
    case RNODE_INPUT:
        free(tree->input.text);
        free(tree->input.placeholder);
        break;
    case RNODE_CHECKBOX:
        free(tree->checkbox.label);
        break;
    case RNODE_TOGGLE:
        free(tree->toggle.label);
        break;
    case RNODE_SPINNER:
        free(tree->spinner.message);
        break;
    case RNODE_BADGE:
        free(tree->badge.text);
        if (tree->badge.style.fg) free(tree->badge.style.fg);
        if (tree->badge.style.bg) free(tree->badge.style.bg);
        break;
    case RNODE_TABLE:
        for (int i = 0; i < tree->table.header_count; i++)
            free(tree->table.headers[i]);
        free(tree->table.headers);
        for (int i = 0; i < tree->table.row_count; i++) {
            for (int j = 0; j < tree->table.header_count; j++)
                free(tree->table.rows[i][j]);
            free(tree->table.rows[i]);
        }
        free(tree->table.rows);
        break;
    case RNODE_GAUGE:
        free(tree->gauge.label);
        break;
    case RNODE_FORM:
        for (int i = 0; i < tree->form.field_count; i++) {
            free(tree->form.fields[i].label);
            free(tree->form.fields[i].widget_type);
            free(tree->form.fields[i].value);
            free(tree->form.fields[i].placeholder);
            for (int j = 0; j < tree->form.fields[i].choice_count; j++)
                free(tree->form.fields[i].choices[j]);
            free(tree->form.fields[i].choices);
        }
        free(tree->form.fields);
        free(tree->form.submit_label);
        break;
    case RNODE_TABS:
        for (int i = 0; i < tree->tabs.tab_count; i++)
            free(tree->tabs.tab_labels[i]);
        free(tree->tabs.tab_labels);
        if (tree->tabs.child) { render_tree_free(tree->tabs.child); free(tree->tabs.child); }
        break;
    case RNODE_SPLIT_PANES:
        if (tree->split_panes.first) { render_tree_free(tree->split_panes.first); free(tree->split_panes.first); }
        if (tree->split_panes.second) { render_tree_free(tree->split_panes.second); free(tree->split_panes.second); }
        break;
    case RNODE_CONTEXT_MENU:
        for (int i = 0; i < tree->context_menu.item_count; i++)
            free(tree->context_menu.items[i].label);
        free(tree->context_menu.items);
        break;
    case RNODE_TOAST:
        free(tree->toast.message);
        break;
    default: break;
    }
}