#include "renderer.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/time.h>
#include "core/animation.h"

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

static int buf_printf(char *buf, int buf_sz, const char *fmt, ...) {
    int len = strlen(buf);
    if (len >= buf_sz - 1) return 0;
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf + len, buf_sz - len, fmt, ap);
    va_end(ap);
    return n;
}

static void set_style(char *buf, int buf_sz, WidgetStyle *ws) {
    if (!ws || ws->fg_color == 0) { buf_printf(buf, buf_sz, "\033[0m"); return; }
    uint8_t r = (ws->fg_color >> 16) & 0xFF;
    uint8_t g = (ws->fg_color >> 8) & 0xFF;
    uint8_t b = ws->fg_color & 0xFF;
    buf_printf(buf, buf_sz, "\033[0");
    if (ws->font_weight >= 700) buf_printf(buf, buf_sz, ";1");
    if (ws->font_italic) buf_printf(buf, buf_sz, ";3");
    buf_printf(buf, buf_sz, ";38;2;%d;%d;%d", r, g, b);
    if (ws->bg_color != 0) {
        r = (ws->bg_color >> 16) & 0xFF;
        g = (ws->bg_color >> 8) & 0xFF;
        b = ws->bg_color & 0xFF;
        buf_printf(buf, buf_sz, ";48;2;%d;%d;%d", r, g, b);
    }
    buf_printf(buf, buf_sz, "m");
}

static void set_dim_style(char *buf, int buf_sz, WidgetStyle *ws, float amount) {
    if (!ws || ws->fg_color == 0) { buf_printf(buf, buf_sz, "\033[0m"); return; }
    uint8_t r = (uint8_t)(((ws->fg_color >> 16) & 0xFF) * amount);
    uint8_t g = (uint8_t)(((ws->fg_color >> 8) & 0xFF) * amount);
    uint8_t b = (uint8_t)((ws->fg_color & 0xFF) * amount);
    buf_printf(buf, buf_sz, "\033[0");
    if (ws->font_weight >= 700) buf_printf(buf, buf_sz, ";1");
    buf_printf(buf, buf_sz, ";38;2;%d;%d;%d", r, g, b);
    if (ws->bg_color != 0) {
        r = (uint8_t)(((ws->bg_color >> 16) & 0xFF) * amount);
        g = (uint8_t)(((ws->bg_color >> 8) & 0xFF) * amount);
        b = (uint8_t)((ws->bg_color & 0xFF) * amount);
        buf_printf(buf, buf_sz, ";48;2;%d;%d;%d", r, g, b);
    }
    buf_printf(buf, buf_sz, "m");
}

static int min(int a, int b) { return a < b ? a : b; }

static void draw_box(char *buf, int buf_sz, int x, int y, int w, int h,
                     BorderStyle border, WidgetStyle *ws) {
    if (w < 2 || h < 2) return;
    const char *tl, *tr, *bl, *br, *h_line, *v_line;
    switch (border) {
        case BORDER_SINGLE: tl="┌"; tr="┐"; bl="└"; br="┘"; h_line="─"; v_line="│"; break;
        case BORDER_DOUBLE: tl="╔"; tr="╗"; bl="╚"; br="╝"; h_line="═"; v_line="║"; break;
        case BORDER_ROUNDED: tl="╭"; tr="╮"; bl="╰"; br="╯"; h_line="─"; v_line="│"; break;
        case BORDER_DASHED: tl="┌"; tr="┐"; bl="└"; br="┘"; h_line="╌"; v_line="╎"; break;
        case BORDER_DOTTED: tl="┌"; tr="┐"; bl="└"; br="┘"; h_line="┈"; v_line="┊"; break;
        default: return;
    }
    set_style(buf, buf_sz, ws);
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
                              const char *text, WidgetStyle *ws, int visible_chars) {
    if (!text || w <= 0 || h <= 0) return;
    if (visible_chars == 0) return;
    set_style(buf, buf_sz, ws);
    int line = 0;
    const char *p = text;
    int chars_left = (visible_chars < 0) ? 999999 : visible_chars;
    while (*p && line < h && chars_left > 0) {
        const char *end = p;
        int col = 0;
        while (*end && *end != '\n' && col < w && col < chars_left) { end++; col++; }
        if (col == w && *end && *end != '\n') {
            const char *space = end;
            while (space > p && *space != ' ') space--;
            if (space > p) { end = space; col = (int)(space - p); }
        }
        int pad = 0;
        if (ws->text_align == ALIGN_CENTER) pad = (w - col) / 2;
        else if (ws->text_align == ALIGN_RIGHT) pad = w - col;
        if (pad < 0) pad = 0;
        buf_printf(buf, buf_sz, "\033[%d;%dH", y+1+line, x+1);
        for (int i = 0; i < pad; i++) buf_printf(buf, buf_sz, " ");
        for (const char *c = p; c < end; c++) buf_printf(buf, buf_sz, "%c", *c);
        for (int i = pad + col; i < w; i++) buf_printf(buf, buf_sz, " ");
        line++;
        chars_left -= col;
        p = (*end == '\n') ? end + 1 : end;
    }
    for (; line < h; line++) {
        buf_printf(buf, buf_sz, "\033[%d;%dH", y+1+line, x+1);
        for (int i = 0; i < w; i++) buf_printf(buf, buf_sz, " ");
    }
    set_style(buf, buf_sz, NULL);
}

static void draw_list(char *buf, int buf_sz, int x, int y, int w, int h,
                      ListItem *items, int count, int selected, WidgetStyle *ws) {
    if (h <= 0 || w <= 0) return;
    int scroll = 0;
    if (selected >= h) scroll = selected - h + 1;
    if (selected < scroll) scroll = selected;
    for (int i = 0; i < h; i++) {
        buf_printf(buf, buf_sz, "\033[%d;%dH", y + 1 + i, x + 1);
        if (i + scroll < count) {
            int idx = i + scroll;
            bool is_sel = (idx == selected);
            if (is_sel) {
                buf_printf(buf, buf_sz, "\033[48;2;%d;%d;%dm\033[38;2;255;255;255m",
                    (ws->accent_color >> 16) & 0xFF,
                    (ws->accent_color >> 8) & 0xFF,
                    ws->accent_color & 0xFF);
            } else {
                set_style(buf, buf_sz, ws);
            }
            const char *label = items[idx].label;
            int len = min(strlen(label), w - 2);
            buf_printf(buf, buf_sz, " %.*s", len, label);
            for (int j = len + 1; j < w; j++) buf_printf(buf, buf_sz, " ");
        } else {
            set_style(buf, buf_sz, ws);
            for (int j = 0; j < w; j++) buf_printf(buf, buf_sz, " ");
        }
        set_style(buf, buf_sz, NULL);
    }
}

static void draw_calendar(char *buf, int buf_sz, int x, int y, int w,
                          int year, int month, int selected_day, WidgetStyle *ws) {
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

    char title[32];
    snprintf(title, sizeof(title), "%s %d", months[month-1], year);
    int title_pad = (w - strlen(title)) / 2;
    if (title_pad < 0) title_pad = 0;
    buf_printf(buf, buf_sz, "\033[%d;%dH", y+1, x+1);
    set_style(buf, buf_sz, ws);
    for (int i = 0; i < title_pad; i++) buf_printf(buf, buf_sz, " ");
    buf_printf(buf, buf_sz, "%s", title);
    set_style(buf, buf_sz, NULL);

    const char *dow = "Su Mo Tu We Th Fr Sa";
    int dow_pad = (w - strlen(dow)) / 2;
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
            buf_printf(buf, buf_sz, "\033[48;2;%d;%d;%dm\033[38;2;255;255;255m",
                (ws->accent_color >> 16) & 0xFF,
                (ws->accent_color >> 8) & 0xFF,
                ws->accent_color & 0xFF);
        }
        buf_printf(buf, buf_sz, "%2d", day);
        set_style(buf, buf_sz, NULL);
    }
}

typedef struct { int depth; TreeNode *node; } Flat;

static void flatten2(TreeNode *n, int c, int d, Flat *flat, int *vis) {
    for (int i = 0; i < c; i++) {
        flat[*vis] = (Flat){d, &n[i]};
        (*vis)++;
        if (n[i].expanded && n[i].child_count > 0)
            flatten2((TreeNode *)n[i].children, n[i].child_count, d+1, flat, vis);
    }
}

static bool has_tui_animation(RenderTree *node) {
    if (!node) return false;
    for (int i = 0; i < node->animation_count; i++) {
        if (node->active_animations[i].playing && !node->active_animations[i].paused &&
            node->active_animations[i].def && node->active_animations[i].def->tui_type != TUI_ANIM_NONE)
            return true;
    }
    return false;
}

static int get_slide_offset_x(RenderTree *node, long long now_ms) {
    for (int i = 0; i < node->animation_count; i++) {
        struct AnimInstance *inst = &node->active_animations[i];
        if (!inst->playing || inst->paused || !inst->def || inst->def->tui_type != TUI_ANIM_SLIDE_IN)
            continue;
        float progress = (float)(now_ms - inst->start_time) / inst->def->duration_ms;
        if (progress < 0.0f) progress = 0.0f;
        if (progress > 1.0f) progress = 1.0f;
        float eased = easing_apply(EASE_OUT, progress);
        return (int)(inst->slide_origin_x * (1.0f - eased));
    }
    return 0;
}

static int get_slide_offset_y(RenderTree *node, long long now_ms) {
    for (int i = 0; i < node->animation_count; i++) {
        struct AnimInstance *inst = &node->active_animations[i];
        if (!inst->playing || inst->paused || !inst->def || inst->def->tui_type != TUI_ANIM_SLIDE_IN)
            continue;
        float progress = (float)(now_ms - inst->start_time) / inst->def->duration_ms;
        if (progress < 0.0f) progress = 0.0f;
        if (progress > 1.0f) progress = 1.0f;
        float eased = easing_apply(EASE_OUT, progress);
        return (int)(inst->slide_origin_y * (1.0f - eased));
    }
    return 0;
}

static float get_fade_amount(RenderTree *node, long long now_ms) {
    for (int i = 0; i < node->animation_count; i++) {
        struct AnimInstance *inst = &node->active_animations[i];
        if (!inst->playing || inst->paused || !inst->def || inst->def->tui_type != TUI_ANIM_FADE)
            continue;
        float progress = (float)(now_ms - inst->start_time) / inst->def->duration_ms;
        if (progress < 0.0f) progress = 0.0f;
        if (progress > 1.0f) progress = 1.0f;
        return progress;
    }
    return 1.0f;
}

static int get_typewriter_chars(RenderTree *node) {
    for (int i = 0; i < node->animation_count; i++) {
        struct AnimInstance *inst = &node->active_animations[i];
        if (!inst->playing || inst->paused || !inst->def || inst->def->tui_type != TUI_ANIM_TYPEWRITER)
            continue;
        return inst->typewriter_pos;
    }
    return -1;
}

static void render_node(RenderTree *node, int off_x, int off_y,
                        int max_w, int max_h, char *buf, int buf_sz, bool parent_dirty,
                        long long now_ms) {
    if (!node || node->rect.w <= 0 || node->rect.h <= 0) return;
    if (!node->dirty && !parent_dirty && !has_tui_animation(node)) return;
    bool self_dirty = node->dirty;
    node->dirty = false;
    int slide_x = get_slide_offset_x(node, now_ms);
    int slide_y = get_slide_offset_y(node, now_ms);
    float fade = get_fade_amount(node, now_ms);
    int typewriter = get_typewriter_chars(node);

    int x = off_x + node->rect.x + slide_x, y = off_y + node->rect.y + slide_y;
    int w = min(node->rect.w, max_w - node->rect.x), h = min(node->rect.h, max_h - node->rect.y);
    if (w <= 0 || h <= 0) return;
    WidgetStyle *ws = &node->resolved_style;

    if (fade < 0.05f) return;

    switch (node->type) {
    case RNODE_CONTAINER:
        if (node->u.container.border != BORDER_NONE) {
            draw_box(buf, buf_sz, x, y, w, h, node->u.container.border, ws);
            fill_rect(buf, buf_sz, x+1, y+1, w-2, h-2);
        }
        for (int i = 0; i < node->u.container.child_count; i++) {
            RenderTree *child = &node->u.container.children[i];
            int cox = x + node->u.container.padding.left;
            int coy = y + node->u.container.padding.top;
            int cw = w - node->u.container.padding.left - node->u.container.padding.right;
            int ch = h - node->u.container.padding.top - node->u.container.padding.bottom;
            render_node(child, cox, coy, cw, ch, buf, buf_sz, self_dirty || parent_dirty, now_ms);
        }
        break;
    case RNODE_FLEX:
        if (node->u.flex.children) {
            for (int i = 0; i < node->u.flex.child_count; i++)
                render_node(&node->u.flex.children[i], x, y, w, h, buf, buf_sz, self_dirty || parent_dirty, now_ms);
        }
        break;
    case RNODE_GRID:
        if (node->u.grid.children) {
            for (int i = 0; i < node->u.grid.child_count; i++)
                render_node(&node->u.grid.children[i], x, y, w, h, buf, buf_sz, self_dirty || parent_dirty, now_ms);
        }
        break;
    case RNODE_TEXT:
        if (fade < 1.0f) {
            WidgetStyle dim = *ws;
            set_dim_style(buf, buf_sz, &dim, fade);
        }
        draw_text_wrapped(buf, buf_sz, x, y, w, h, node->u.text.content, ws, typewriter);
        break;
    case RNODE_LIST:
        draw_list(buf, buf_sz, x, y, w, h, node->u.list.items, node->u.list.item_count, node->u.list.selected, ws);
        break;
    case RNODE_INPUT:
        set_style(buf, buf_sz, ws);
        buf_printf(buf, buf_sz, "\033[%d;%dH", y+1, x+1);
        for (int i = 0; i < w; i++) buf_printf(buf, buf_sz, " ");
        buf_printf(buf, buf_sz, "\033[%d;%dH", y+1, x+1);
        if (node->u.input.masked) {
            int len = node->u.input.text ? strlen(node->u.input.text) : 0;
            for (int i = 0; i < min(len, w-2); i++) buf_printf(buf, buf_sz, "*");
            if (len == 0) buf_printf(buf, buf_sz, "%s", node->u.input.placeholder ? node->u.input.placeholder : "");
        } else {
            const char *t = node->u.input.text && strlen(node->u.input.text) ? node->u.input.text : (node->u.input.placeholder ? node->u.input.placeholder : "");
            buf_printf(buf, buf_sz, "> %.*s", w-4, t);
        }
        set_style(buf, buf_sz, NULL);
        break;
    case RNODE_CHECKBOX:
        set_style(buf, buf_sz, ws);
        buf_printf(buf, buf_sz, "\033[%d;%dH%s %s", y+1, x+1, node->u.checkbox.checked ? "[x]" : "[ ]", node->u.checkbox.label);
        set_style(buf, buf_sz, NULL);
        break;
    case RNODE_TOGGLE:
        set_style(buf, buf_sz, ws);
        buf_printf(buf, buf_sz, "\033[%d;%dH[ %s ] %s", y+1, x+1, node->u.toggle.value ? "ON" : "OFF", node->u.toggle.label);
        set_style(buf, buf_sz, NULL);
        break;
    case RNODE_SPINNER: {
        const char *frames[] = {"⠋","⠙","⠹","⠸","⠼","⠴","⠦","⠧","⠇","⠏"};
        set_style(buf, buf_sz, ws);
        buf_printf(buf, buf_sz, "\033[%d;%dH%s %s", y+1, x+1, frames[node->u.spinner.frame % 10], node->u.spinner.message);
        set_style(buf, buf_sz, NULL);
        break;
    }
    case RNODE_SEPARATOR:
        if (node->u.separator.orientation == ORIENT_HORIZONTAL)
            for (int i = 0; i < w; i++) buf_printf(buf, buf_sz, "\033[%d;%dH─", y+1, x+i+1);
        else
            for (int i = 0; i < h; i++) buf_printf(buf, buf_sz, "\033[%d;%dH│", y+i+1, x+1);
        break;
    case RNODE_BADGE:
        set_style(buf, buf_sz, ws);
        buf_printf(buf, buf_sz, "\033[%d;%dH%s", y+1, x+1, node->u.badge.text);
        set_style(buf, buf_sz, NULL);
        break;
    case RNODE_CURSOR:
        buf_printf(buf, buf_sz, "\033[%d;%dH\033[7m \033[0m", y + node->u.cursor.y + 1, x + node->u.cursor.x + 1);
        break;
    case RNODE_TABLE:
        buf_printf(buf, buf_sz, "\033[%d;%dH", y+1, x+1);
        set_style(buf, buf_sz, ws);
        for (int i = 0; i < node->u.table.header_count; i++) buf_printf(buf, buf_sz, "%-20s", node->u.table.headers[i]);
        set_style(buf, buf_sz, NULL);
        for (int r = 0; r < node->u.table.row_count && r < h-1; r++) {
            buf_printf(buf, buf_sz, "\033[%d;%dH", y+r+2, x+1);
            bool sel = (node->u.table.selected_row == r);
            if (sel) {
                buf_printf(buf, buf_sz, "\033[48;2;%d;%d;%dm\033[38;2;255;255;255m",
                    (ws->accent_color>>16)&0xFF, (ws->accent_color>>8)&0xFF, ws->accent_color&0xFF);
            }
            for (int c = 0; c < node->u.table.header_count; c++)
                buf_printf(buf, buf_sz, "%-20s", node->u.table.rows[r][c]);
            set_style(buf, buf_sz, NULL);
        }
        break;
    case RNODE_TREE: {
        Flat flat[256];
        int vis = 0;
        flatten2(node->u.tree.nodes, node->u.tree.node_count, 0, flat, &vis);
        int scroll = 0, s = node->u.tree.selected_path ? node->u.tree.selected_path[0] : 0;
        if (s >= h) scroll = s - h + 1;
        for (int i = 0; i < h && i + scroll < vis; i++) {
            Flat *f = &flat[i + scroll];
            buf_printf(buf, buf_sz, "\033[%d;%dH", y+1+i, x+1);
            set_style(buf, buf_sz, ws);
            buf_printf(buf, buf_sz, "%*s%s%s", f->depth*2, "",
                f->node->child_count > 0 ? (f->node->expanded ? "▼ " : "▶ ") : "  ",
                f->node->label);
            set_style(buf, buf_sz, NULL);
        }
        break;
    }
    case RNODE_GAUGE:
        set_style(buf, buf_sz, ws);
        buf_printf(buf, buf_sz, "\033[%d;%dH%s %d%%", y+1, x+1, node->u.gauge.label, node->u.gauge.percent);
        buf_printf(buf, buf_sz, "\033[%d;%dH", y+2, x+1);
        { int f = (w * node->u.gauge.percent) / 100;
        for (int i = 0; i < f; i++) buf_printf(buf, buf_sz, "=");
        for (int i = f; i < w; i++) buf_printf(buf, buf_sz, "-"); }
        set_style(buf, buf_sz, NULL);
        break;
    case RNODE_CALENDAR:
        draw_calendar(buf, buf_sz, x, y, w, node->u.calendar.year, node->u.calendar.month, node->u.calendar.selected_day, ws);
        break;
    case RNODE_FORM:
        for (int i = 0; i < node->u.form.field_count && i < h; i++) {
            buf_printf(buf, buf_sz, "\033[%d;%dH", y+1+i, x+1);
            set_style(buf, buf_sz, ws);
            buf_printf(buf, buf_sz, "%s: %s", node->u.form.fields[i].label, node->u.form.fields[i].value);
            set_style(buf, buf_sz, NULL);
        }
        if (node->u.form.field_count < h) {
            buf_printf(buf, buf_sz, "\033[%d;%dH", y + node->u.form.field_count + 1, x+1);
            set_style(buf, buf_sz, ws);
            buf_printf(buf, buf_sz, "[ %s ]", node->u.form.submit_label);
            set_style(buf, buf_sz, NULL);
        }
        break;
    case RNODE_TABS: {
        int lw = 0;
        for (int i = 0; i < node->u.tabs.tab_count; i++) {
            buf_printf(buf, buf_sz, "\033[%d;%dH", y+1, x+1+lw);
            set_style(buf, buf_sz, ws);
            buf_printf(buf, buf_sz, i == node->u.tabs.active ? "[ %s ]" : "  %s  ", node->u.tabs.tab_labels[i]);
            set_style(buf, buf_sz, NULL);
            lw += strlen(node->u.tabs.tab_labels[i]) + 4;
        }
        if (node->u.tabs.child && h > 1)
            render_node(node->u.tabs.child, x, y+1, w, h-1, buf, buf_sz, self_dirty || parent_dirty, now_ms);
        break;
    }
    case RNODE_SPLIT_PANES: {
        int sp = node->u.split_panes.split_position > 0 ? node->u.split_panes.split_position :
                  (node->u.split_panes.orientation == ORIENT_HORIZONTAL ? w/2 : h/2);
        if (node->u.split_panes.orientation == ORIENT_HORIZONTAL) {
            if (node->u.split_panes.first) render_node(node->u.split_panes.first, x, y, sp, h, buf, buf_sz, self_dirty || parent_dirty, now_ms);
            for (int iy = 0; iy < h; iy++) buf_printf(buf, buf_sz, "\033[%d;%dH│", y+1+iy, x+sp+1);
            if (node->u.split_panes.second) render_node(node->u.split_panes.second, x+sp+1, y, w-sp-1, h, buf, buf_sz, self_dirty || parent_dirty, now_ms);
        } else {
            if (node->u.split_panes.first) render_node(node->u.split_panes.first, x, y, w, sp, buf, buf_sz, self_dirty || parent_dirty, now_ms);
            buf_printf(buf, buf_sz, "\033[%d;%dH", y+sp+1, x+1);
            for (int ix = 0; ix < w; ix++) buf_printf(buf, buf_sz, "─");
            if (node->u.split_panes.second) render_node(node->u.split_panes.second, x, y+sp+1, w, h-sp-1, buf, buf_sz, self_dirty || parent_dirty, now_ms);
        }
        break;
    }
    case RNODE_CONTEXT_MENU:
        for (int i = 0; i < node->u.context_menu.item_count && i < h; i++) {
            buf_printf(buf, buf_sz, "\033[%d;%dH", y+1+i, x+1);
            set_style(buf, buf_sz, ws);
            buf_printf(buf, buf_sz, " %s ", node->u.context_menu.items[i].label);
            set_style(buf, buf_sz, NULL);
        }
        break;
    case RNODE_TOAST:
        buf_printf(buf, buf_sz, "\033[%d;%dH", y+h-1, x+1);
        set_style(buf, buf_sz, ws);
        buf_printf(buf, buf_sz, " %s ", node->u.toast.message);
        set_style(buf, buf_sz, NULL);
        break;
    case RNODE_VECTOR:
        set_style(buf, buf_sz, ws);
        buf_printf(buf, buf_sz, "\033[%d;%dH[Vector: %s]", y+1, x+1,
            node->u.vector.path ? node->u.vector.path : "");
        set_style(buf, buf_sz, NULL);
        break;
    case RNODE_RICH_TEXT:
        if (node->u.rich_text.spans && node->u.rich_text.spans[0]) {
            WidgetStyle local = *ws;
            local.text_align = ALIGN_LEFT;
            draw_text_wrapped(buf, buf_sz, x, y, w, h, node->u.rich_text.spans, &local, -1);
        }
        break;
    case RNODE_IMAGE:
        set_style(buf, buf_sz, ws);
        buf_printf(buf, buf_sz, "\033[%d;%dH[Image: %s]", y+1, x+1,
            node->u.image.source ? node->u.image.source : "");
        set_style(buf, buf_sz, NULL);
        break;
    case RNODE_CANVAS:
        set_style(buf, buf_sz, ws);
        buf_printf(buf, buf_sz, "\033[%d;%dH[Canvas]", y+1, x+1);
        set_style(buf, buf_sz, NULL);
        break;
    case RNODE_MARKDOWN:
        if (node->u.markdown.content && node->u.markdown.content[0]) {
            WidgetStyle local = *ws;
            local.text_align = ALIGN_LEFT;
            draw_text_wrapped(buf, buf_sz, x, y, w, h, node->u.markdown.content, &local, -1);
        }
        break;
    case RNODE_PLOT:
        set_style(buf, buf_sz, ws);
        buf_printf(buf, buf_sz, "\033[%d;%dH[Plot: %s]", y+1, x+1,
            node->u.plot.type ? node->u.plot.type : "line");
        set_style(buf, buf_sz, NULL);
        break;
    case RNODE_VIDEO:
        set_style(buf, buf_sz, ws);
        buf_printf(buf, buf_sz, "\033[%d;%dH[Video]", y+1, x+1);
        set_style(buf, buf_sz, NULL);
        break;
    }
}

void render_tree_to_buf(RenderTree *tree, int off_x, int off_y, int max_w, int max_h, char *buf, int buf_sz) {
    buf[0] = '\0';
    if (!tree) return;
    buf_printf(buf, buf_sz, "\033[2J\033[H");
    long long now_ms = 0;
    struct timeval tv;
    if (gettimeofday(&tv, NULL) == 0) now_ms = (long long)tv.tv_sec * 1000 + tv.tv_usec / 1000;
    render_node(tree, off_x, off_y, max_w, max_h, buf, buf_sz, true, now_ms);
    buf_printf(buf, buf_sz, "\033[0m");
}