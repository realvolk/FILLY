#include "renderer.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

static int buf_printf(char *buf, int buf_sz, const char *fmt, ...) {
    int len = strlen(buf);
    if (len >= buf_sz - 1) return 0;
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf + len, buf_sz - len, fmt, ap);
    va_end(ap);
    return n;
}

static void set_style(char *buf, int buf_sz, TextStyle *s) {
    if (!s) { buf_printf(buf, buf_sz, "\033[0m"); return; }
    buf_printf(buf, buf_sz, "\033[0");
    if (s->bold) buf_printf(buf, buf_sz, ";1");
    if (s->italic) buf_printf(buf, buf_sz, ";3");
    if (s->underline) buf_printf(buf, buf_sz, ";4");
    if (s->fg) buf_printf(buf, buf_sz, ";38;5;%s", s->fg);
    if (s->bg) buf_printf(buf, buf_sz, ";48;5;%s", s->bg);
    buf_printf(buf, buf_sz, "m");
}

static int min(int a, int b) { return a < b ? a : b; }

static void draw_box(char *buf, int buf_sz, int x, int y, int w, int h,
                     BorderStyle border, const char *border_color) {
    if (w < 2 || h < 2) return;
    const char *tl, *tr, *bl, *br, *h_line, *v_line;
    switch (border) {
        case BORDER_SINGLE: tl="┌"; tr="┐"; bl="└"; br="┘"; h_line="─"; v_line="│"; break;
        case BORDER_DOUBLE: tl="╔"; tr="╗"; bl="╚"; br="╝"; h_line="═"; v_line="║"; break;
        case BORDER_ROUNDED: tl="╭"; tr="╮"; bl="╰"; br="╯"; h_line="─"; v_line="│"; break;
        default: return;
    }
    TextStyle border_style = { .fg = (char *)border_color, .bg = NULL };
    set_style(buf, buf_sz, &border_style);
    buf_printf(buf, buf_sz, "\033[%d;%dH%s", y+1, x+1, tl);
    for (int i = 1; i < w-1; i++) buf_printf(buf, buf_sz, "%s", h_line);
    buf_printf(buf, buf_sz, "%s", tr);
    for (int i = 1; i < h-1; i++) {
        buf_printf(buf, buf_sz, "\033[%d;%dH%s", y+i+1, x+1, v_line);
        buf_printf(buf, buf_sz, "\033[%d;%dH%s", y+i+1, x+w, v_line);
    }
    buf_printf(buf, buf_sz, "\033[%d;%dH%s", y+h, x+1, bl);
    for (int i = 1; i < w-1; i++) buf_printf(buf, buf_sz, "%s", h_line);
    buf_printf(buf, buf_sz, "%s", br);
    set_style(buf, buf_sz, NULL);
}

static void fill_rect(char *buf, int buf_sz, int x, int y, int w, int h) {
    buf_printf(buf, buf_sz, "\033[0m");
    for (int iy = 0; iy < h; iy++) {
        buf_printf(buf, buf_sz, "\033[%d;%dH", y+1+iy, x+1);
        for (int ix = 0; ix < w; ix++) buf_printf(buf, buf_sz, " ");
    }
}

static void draw_text_wrapped(char *buf, int buf_sz, int x, int y, int w, int h,
                              const char *text, Alignment align, TextStyle *style) {
    if (!text || w <= 0 || h <= 0) return;
    set_style(buf, buf_sz, style);
    int line = 0;
    const char *p = text;
    while (*p && line < h) {
        const char *end = p;
        int col = 0;
        while (*end && *end != '\n' && col < w) { end++; col++; }
        if (col == w && *end && *end != '\n') {
            const char *space = end;
            while (space > p && *space != ' ') space--;
            if (space > p) { end = space; col = (int)(space - p); }
        }
        int pad = 0;
        if (align == ALIGN_CENTER) pad = (w - col) / 2;
        else if (align == ALIGN_RIGHT) pad = w - col;
        if (pad < 0) pad = 0;
        buf_printf(buf, buf_sz, "\033[%d;%dH", y+1+line, x+1);
        for (int i = 0; i < pad; i++) buf_printf(buf, buf_sz, " ");
        for (const char *c = p; c < end; c++) buf_printf(buf, buf_sz, "%c", *c);
        for (int i = pad + col; i < w; i++) buf_printf(buf, buf_sz, " ");
        line++;
        p = (*end == '\n') ? end + 1 : end;
    }
    for (; line < h; line++) {
        buf_printf(buf, buf_sz, "\033[%d;%dH", y+1+line, x+1);
        for (int i = 0; i < w; i++) buf_printf(buf, buf_sz, " ");
    }
    set_style(buf, buf_sz, NULL);
}

static void draw_list(char *buf, int buf_sz, int x, int y, int w, int h,
                      ListItem *items, int count, int selected,
                      TextStyle *highlight) {
    if (h <= 0 || w <= 0) return;
    int scroll = 0;
    if (selected >= h) scroll = selected - h + 1;
    if (selected < scroll) scroll = selected;
    TextStyle normal = textstyle_normal();
    for (int i = 0; i < h; i++) {
        buf_printf(buf, buf_sz, "\033[%d;%dH", y + 1 + i, x + 1);
        if (i + scroll < count) {
            int idx = i + scroll;
            bool is_sel = (idx == selected);
            TextStyle *s = is_sel ? highlight : &normal;
            set_style(buf, buf_sz, s);
            const char *label = items[idx].label;
            int len = min(strlen(label), w - 2);
            buf_printf(buf, buf_sz, " %.*s", len, label);
            for (int j = len + 1; j < w; j++) buf_printf(buf, buf_sz, " ");
        } else {
            set_style(buf, buf_sz, &normal);
            for (int j = 0; j < w; j++) buf_printf(buf, buf_sz, " ");
        }
        set_style(buf, buf_sz, NULL);
    }
}

static void draw_calendar(char *buf, int buf_sz, int x, int y, int w,
                          int year, int month, int selected_day,
                          TextStyle *highlight) {
    const char *months[] = {"Jan","Feb","Mar","Apr","May","Jun",
                            "Jul","Aug","Sep","Oct","Nov","Dec"};
    int dim = 31;
    switch (month) {
        case 4: case 6: case 9: case 11: dim = 30; break;
        case 2: dim = (year % 4 == 0 && (year % 100 || year % 400 == 0)) ? 29 : 28; break;
    }
    int m = (month < 3) ? month + 12 : month;
    int y_adj = (month < 3) ? year - 1 : year;
    int h = (1 + (13*(m+1))/5 + y_adj + y_adj/4 - y_adj/100 + y_adj/400) % 7;
    int first_dow = (h + 6) % 7;
    TextStyle hdr = textstyle_selected();

    char title[32];
    snprintf(title, sizeof(title), "%s %d", months[month-1], year);
    int title_len = strlen(title);
    int title_pad = (w - title_len) / 2;
    if (title_pad < 0) title_pad = 0;
    buf_printf(buf, buf_sz, "\033[%d;%dH", y+1, x+1);
    set_style(buf, buf_sz, &hdr);
    for (int i = 0; i < title_pad; i++) buf_printf(buf, buf_sz, " ");
    buf_printf(buf, buf_sz, "%s", title);
    set_style(buf, buf_sz, NULL);

    const char *dow = "Su Mo Tu We Th Fr Sa";
    int dow_len = strlen(dow);
    int dow_pad = (w - dow_len) / 2;
    if (dow_pad < 0) dow_pad = 0;
    buf_printf(buf, buf_sz, "\033[%d;%dH", y+2, x+1);
    for (int i = 0; i < dow_pad; i++) buf_printf(buf, buf_sz, " ");
    buf_printf(buf, buf_sz, "%s", dow);

    for (int day = 1; day <= dim; day++) {
        int dow_idx = (first_dow + day - 1) % 7;
        int row = 2 + (first_dow + day - 1) / 7;
        int col = 1 + dow_pad + dow_idx * 3;
        buf_printf(buf, buf_sz, "\033[%d;%dH", y + row + 1, x + col);
        if (day == selected_day) {
            set_style(buf, buf_sz, highlight);
            buf_printf(buf, buf_sz, "%2d", day);
            set_style(buf, buf_sz, NULL);
        } else buf_printf(buf, buf_sz, "%2d", day);
    }
}

static void render_node(RenderTree *node, int off_x, int off_y,
                        int max_w, int max_h,
                        char *buf, int buf_sz) {
    if (!node || node->rect.w <= 0 || node->rect.h <= 0) return;
    int x = off_x + node->rect.x;
    int y = off_y + node->rect.y;
    int w = min(node->rect.w, max_w - node->rect.x);
    int h = min(node->rect.h, max_h - node->rect.y);
    if (w <= 0 || h <= 0) return;

    TextStyle normal = textstyle_normal();
    TextStyle sel = textstyle_selected();
    TextStyle accent = textstyle_accent();

    switch (node->type) {
    case RNODE_CONTAINER:
        if (node->container.bg) {
            TextStyle bg_style = { .fg = NULL, .bg = node->container.bg };
            set_style(buf, buf_sz, &bg_style);
            for (int iy = 0; iy < h; iy++) {
                buf_printf(buf, buf_sz, "\033[%d;%dH", y+1+iy, x+1);
                for (int ix = 0; ix < w; ix++) buf_printf(buf, buf_sz, " ");
            }
            set_style(buf, buf_sz, NULL);
        }
        if (node->container.border != BORDER_NONE) {
            draw_box(buf, buf_sz, x, y, w, h, node->container.border, "8");
            fill_rect(buf, buf_sz, x+1, y+1, w-2, h-2);
        }
        for (int i = 0; i < node->container.child_count; i++) {
            RenderTree *child = &node->container.children[i];
            int child_off_x = x + node->container.padding.left;
            int child_off_y = y + node->container.padding.top;
            int child_w = w - node->container.padding.left - node->container.padding.right;
            int child_h = h - node->container.padding.top - node->container.padding.bottom;
            render_node(child, child_off_x, child_off_y, child_w, child_h, buf, buf_sz);
        }
        break;
    case RNODE_TEXT:
        draw_text_wrapped(buf, buf_sz, x, y, w, h, node->text.content, node->text.align, &node->text.style);
        break;
    case RNODE_LIST:
        draw_list(buf, buf_sz, x, y, w, h, node->list.items, node->list.item_count, node->list.selected, &node->list.highlight);
        break;
    case RNODE_INPUT:
        set_style(buf, buf_sz, &normal);
        buf_printf(buf, buf_sz, "\033[%d;%dH", y+1, x+1);
        for (int i = 0; i < w; i++) buf_printf(buf, buf_sz, " ");
        buf_printf(buf, buf_sz, "\033[%d;%dH", y+1, x+1);
        if (node->input.masked) {
            int len = node->input.text ? strlen(node->input.text) : 0;
            for (int i = 0; i < min(len, w-2); i++) buf_printf(buf, buf_sz, "*");
            if (len == 0) buf_printf(buf, buf_sz, "%s", node->input.placeholder ? node->input.placeholder : "");
        } else {
            const char *t = node->input.text && strlen(node->input.text) ? node->input.text : (node->input.placeholder ? node->input.placeholder : "");
            buf_printf(buf, buf_sz, "> %.*s", w-4, t);
        }
        set_style(buf, buf_sz, NULL);
        if (node->input.text && !node->input.masked) {
            int cx = x + 2 + node->input.cursor;
            if (cx < x + w) buf_printf(buf, buf_sz, "\033[%d;%dH", y+1, cx+1);
        }
        break;
    case RNODE_CHECKBOX:
        set_style(buf, buf_sz, &normal);
        buf_printf(buf, buf_sz, "\033[%d;%dH", y+1, x+1);
        buf_printf(buf, buf_sz, "%s %s", node->checkbox.checked ? "[x]" : "[ ]", node->checkbox.label);
        set_style(buf, buf_sz, NULL);
        break;
    case RNODE_TOGGLE:
        set_style(buf, buf_sz, &normal);
        buf_printf(buf, buf_sz, "\033[%d;%dH", y+1, x+1);
        buf_printf(buf, buf_sz, "[ %s ] %s", node->toggle.value ? "ON" : "OFF", node->toggle.label);
        set_style(buf, buf_sz, NULL);
        break;
    case RNODE_SPINNER: {
        const char *frames[] = {"⠋","⠙","⠹","⠸","⠼","⠴","⠦","⠧","⠇","⠏"};
        set_style(buf, buf_sz, &normal);
        buf_printf(buf, buf_sz, "\033[%d;%dH", y+1, x+1);
        buf_printf(buf, buf_sz, "%s %s", frames[node->spinner.frame % 10], node->spinner.message);
        set_style(buf, buf_sz, NULL);
        break;
    }
    case RNODE_SEPARATOR:
        if (node->separator.orientation == ORIENT_HORIZONTAL)
            for (int i = 0; i < w; i++) buf_printf(buf, buf_sz, "\033[%d;%dH─", y+1, x+i+1);
        else
            for (int i = 0; i < h; i++) buf_printf(buf, buf_sz, "\033[%d;%dH│", y+i+1, x+1);
        break;
    case RNODE_BADGE:
        set_style(buf, buf_sz, &node->badge.style);
        buf_printf(buf, buf_sz, "\033[%d;%dH", y+1, x+1);
        buf_printf(buf, buf_sz, "%s", node->badge.text);
        set_style(buf, buf_sz, NULL);
        break;
    case RNODE_CURSOR:
        buf_printf(buf, buf_sz, "\033[%d;%dH", y + node->cursor.y + 1, x + node->cursor.x + 1);
        buf_printf(buf, buf_sz, "\033[7m \033[0m");
        break;
    case RNODE_TABLE:
        buf_printf(buf, buf_sz, "\033[%d;%dH", y+1, x+1);
        { TextStyle yl = { .fg = "3" }; set_style(buf, buf_sz, &yl); }
        for (int i = 0; i < node->table.header_count; i++) buf_printf(buf, buf_sz, "%-20s", node->table.headers[i]);
        set_style(buf, buf_sz, NULL);
        for (int r = 0; r < node->table.row_count && r < h-1; r++) {
            buf_printf(buf, buf_sz, "\033[%d;%dH", y+r+2, x+1);
            for (int c = 0; c < node->table.header_count; c++) {
                bool cs = (node->table.selected_row == r && node->table.selected_col == c);
                set_style(buf, buf_sz, cs ? &node->table.highlight : &normal);
                buf_printf(buf, buf_sz, "%-20s", node->table.rows[r][c]);
            }
            set_style(buf, buf_sz, NULL);
        }
        break;
    case RNODE_TREE: {
        int vis = 0;
        typedef struct { int depth; TreeNode *node; } Flat;
        Flat flat[256];
        void flatten2(TreeNode *n, int c, int d) {
            for (int i = 0; i < c; i++) {
                flat[vis++] = (Flat){d, &n[i]};
                if (n[i].expanded && n[i].child_count > 0)
                    flatten2((TreeNode *)n[i].children, n[i].child_count, d+1);
            }
        }
        flatten2(node->tree.nodes, node->tree.node_count, 0);
        int scroll = 0, s = node->tree.selected_path ? node->tree.selected_path[0] : 0;
        if (s >= h) scroll = s - h + 1;
        for (int i = 0; i < h && i + scroll < vis; i++) {
            Flat *f = &flat[i + scroll];
            buf_printf(buf, buf_sz, "\033[%d;%dH", y+1+i, x+1);
            set_style(buf, buf_sz, (i+scroll==s) ? &node->tree.highlight : &normal);
            buf_printf(buf, buf_sz, "%*s", f->depth*2, "");
            buf_printf(buf, buf_sz, "%s%s", f->node->child_count > 0 ? (f->node->expanded ? "▼ " : "▶ ") : "  ", f->node->label);
            set_style(buf, buf_sz, NULL);
        }
        break;
    }
    case RNODE_GAUGE:
        set_style(buf, buf_sz, &node->gauge.style);
        buf_printf(buf, buf_sz, "\033[%d;%dH", y+1, x+1);
        buf_printf(buf, buf_sz, "%s %d%%", node->gauge.label, node->gauge.percent);
        buf_printf(buf, buf_sz, "\033[%d;%dH", y+2, x+1);
        { int f = (w * node->gauge.percent) / 100;
        for (int i = 0; i < f; i++) buf_printf(buf, buf_sz, "=");
        for (int i = f; i < w; i++) buf_printf(buf, buf_sz, "-"); }
        set_style(buf, buf_sz, NULL);
        break;
    case RNODE_CALENDAR:
        draw_calendar(buf, buf_sz, x, y, w, node->calendar.year, node->calendar.month, node->calendar.selected_day, &node->calendar.highlight);
        break;
    case RNODE_FORM:
        for (int i = 0; i < node->form.field_count && i < h; i++) {
            buf_printf(buf, buf_sz, "\033[%d;%dH", y+1+i, x+1);
            set_style(buf, buf_sz, (i == node->form.focused) ? &sel : &normal);
            buf_printf(buf, buf_sz, "%s: %s", node->form.fields[i].label, node->form.fields[i].value);
            set_style(buf, buf_sz, NULL);
        }
        if (node->form.field_count < h) {
            buf_printf(buf, buf_sz, "\033[%d;%dH", y + node->form.field_count + 1, x+1);
            set_style(buf, buf_sz, &accent);
            buf_printf(buf, buf_sz, "[ %s ]", node->form.submit_label);
            set_style(buf, buf_sz, NULL);
        }
        break;
    case RNODE_TABS: {
        int lw = 0;
        for (int i = 0; i < node->tabs.tab_count; i++) {
            if (i > 0) buf_printf(buf, buf_sz, " ");
            buf_printf(buf, buf_sz, "\033[%d;%dH", y+1, x+1+lw);
            set_style(buf, buf_sz, (i == node->tabs.active) ? &sel : &normal);
            buf_printf(buf, buf_sz, (i == node->tabs.active) ? "[ %s ]" : "  %s  ", node->tabs.tab_labels[i]);
            set_style(buf, buf_sz, NULL);
            lw += strlen(node->tabs.tab_labels[i]) + 4;
        }
        if (node->tabs.child && h > 1)
            render_node(node->tabs.child, x, y + 1, w, h - 1, buf, buf_sz);
        break;
    }
    case RNODE_SPLIT_PANES: {
        int sp = node->split_panes.split_position > 0 ? node->split_panes.split_position : (node->split_panes.orientation == ORIENT_HORIZONTAL ? w/2 : h/2);
        if (node->split_panes.orientation == ORIENT_HORIZONTAL) {
            render_node(node->split_panes.first, x, y, sp, h, buf, buf_sz);
            for (int iy = 0; iy < h; iy++) buf_printf(buf, buf_sz, "\033[%d;%dH│", y+1+iy, x+sp+1);
            render_node(node->split_panes.second, x+sp+1, y, w-sp-1, h, buf, buf_sz);
        } else {
            render_node(node->split_panes.first, x, y, w, sp, buf, buf_sz);
            buf_printf(buf, buf_sz, "\033[%d;%dH", y+sp+1, x+1);
            for (int ix = 0; ix < w; ix++) buf_printf(buf, buf_sz, "─");
            render_node(node->split_panes.second, x, y+sp+1, w, h-sp-1, buf, buf_sz);
        }
        break;
    }
    case RNODE_CONTEXT_MENU:
        for (int i = 0; i < node->context_menu.item_count && i < h; i++) {
            buf_printf(buf, buf_sz, "\033[%d;%dH", y+1+i, x+1);
            set_style(buf, buf_sz, (i == node->context_menu.selected) ? &node->context_menu.highlight : &normal);
            buf_printf(buf, buf_sz, " %s ", node->context_menu.items[i].label);
            set_style(buf, buf_sz, NULL);
        }
        break;
    case RNODE_TOAST: {
        int tw = min(strlen(node->toast.message) + 4, w);
        int tx = x + (w - tw) / 2;
        int ty = y + h - 1;
        buf_printf(buf, buf_sz, "\033[%d;%dH", ty+1, tx+1);
        set_style(buf, buf_sz, &node->toast.style);
        buf_printf(buf, buf_sz, " %s ", node->toast.message);
        set_style(buf, buf_sz, NULL);
        break;
    }
    }
}

void render_tree_to_buf(RenderTree *tree, int off_x, int off_y, int max_w, int max_h, char *buf, int buf_sz) {
    buf[0] = '\0';
    if (!tree) return;
    buf_printf(buf, buf_sz, "\033[2J\033[H");
    render_node(tree, off_x, off_y, max_w, max_h, buf, buf_sz);
}