#include "text_editor.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct {
    char *title;
    char *file_path;
    char **lines;
    int line_count;
    int row;
    int col;
    int scroll;
    int visible_h;
    bool dirty;
} TextEditorData;

static void te_save(TextEditorData *d) {
    if (!d->file_path) return;
    FILE *f = fopen(d->file_path, "w");
    if (!f) return;
    for (int i = 0; i < d->line_count; i++) fprintf(f, "%s\n", d->lines[i]);
    fclose(f);
}

static void te_clamp_cursor(TextEditorData *d) {
    if (d->line_count == 0) {
        d->lines = realloc(d->lines, sizeof(char *));
        d->lines[0] = strdup("");
        d->line_count = 1;
    }
    if (d->row >= d->line_count) d->row = d->line_count - 1;
    if (d->row < 0) d->row = 0;
    if (d->col > (int)strlen(d->lines[d->row])) d->col = strlen(d->lines[d->row]);
    if (d->col < 0) d->col = 0;
    if (d->row < d->scroll) d->scroll = d->row;
    if (d->row >= d->scroll + d->visible_h) d->scroll = d->row - d->visible_h + 1;
    if (d->scroll < 0) d->scroll = 0;
}

static void te_split_content(TextEditorData *d, const char *content) {
    if (!content) return;
    const char *p = content;
    const char *start = p;
    while (*p) {
        if (*p == '\n') {
            d->lines = realloc(d->lines, (d->line_count + 1) * sizeof(char *));
            int len = p - start;
            d->lines[d->line_count] = malloc(len + 1);
            memcpy(d->lines[d->line_count], start, len);
            d->lines[d->line_count][len] = '\0';
            d->line_count++;
            start = p + 1;
        }
        p++;
    }
    if (p > start || d->line_count == 0) {
        d->lines = realloc(d->lines, (d->line_count + 1) * sizeof(char *));
        int len = p - start;
        d->lines[d->line_count] = malloc(len + 1);
        memcpy(d->lines[d->line_count], start, len);
        d->lines[d->line_count][len] = '\0';
        d->line_count++;
    }
}

static void text_editor_render(Widget *self, Rect area, RenderTree *out) {
    TextEditorData *d = (TextEditorData *)(self + 1);
    d->visible_h = (int)(area.h * 0.9f) - 4;
    if (d->visible_h < 1) d->visible_h = 1;
    te_clamp_cursor(d);
    memset(out, 0, sizeof(*out));
    int box_w = (int)(area.w * 0.9f);
    if (box_w > area.w - 2) box_w = area.w - 2;
    int box_h = (int)(area.h * 0.9f);
    if (box_h > area.h - 2) box_h = area.h - 2;
    int box_x = (area.w - box_w) / 2, box_y = (area.h - box_h) / 2;

    RenderTree *children = calloc(5, sizeof(RenderTree));

    children[0].type = RNODE_TEXT;
    children[0].rect = rect_new(1, 0, box_w - 2, 1);
    char title_buf[512];
    if (d->file_path)
        snprintf(title_buf, sizeof(title_buf), "%s - %s  Ln %d, Col %d", d->title, d->file_path, d->row + 1, d->col + 1);
    else
        snprintf(title_buf, sizeof(title_buf), "%s  Ln %d, Col %d", d->title, d->row + 1, d->col + 1);
    children[0].text.content = strdup(title_buf);
    children[0].text.align = ALIGN_CENTER;
    children[0].text.style = textstyle_selected();

    char *display_text = strdup("");
    int end = d->scroll + d->visible_h;
    if (end > d->line_count) end = d->line_count;
    for (int i = d->scroll; i < end; i++) {
        char *new_text = malloc(strlen(display_text) + strlen(d->lines[i]) + 2);
        sprintf(new_text, "%s%s\n", display_text, d->lines[i]);
        free(display_text);
        display_text = new_text;
    }
    children[1].type = RNODE_TEXT;
    children[1].rect = rect_new(1, 1, box_w - 2, d->visible_h);
    children[1].text.content = display_text;
    children[1].text.align = ALIGN_LEFT;
    children[1].text.style = textstyle_normal();

    children[2].type = RNODE_TEXT;
    children[2].rect = rect_new(1, box_h - 3, box_w - 2, 1);
    children[2].text.content = strdup("^S:save  ^D:delete line  Esc:done  Arrows:move  Enter:newline  BS:del  Home/End/PgUp/PgDn");
    children[2].text.align = ALIGN_CENTER;
    children[2].text.style = textstyle_muted();

    children[3].type = RNODE_CURSOR;
    children[3].rect = rect_new(1, 1, 1, 1);
    children[3].cursor.x = d->col;
    children[3].cursor.y = d->row - d->scroll;

    children[4].type = RNODE_TEXT;
    children[4].rect = rect_new(1, box_h - 2, box_w - 2, 1);
    children[4].text.content = strdup("");
    children[4].text.align = ALIGN_LEFT;
    children[4].text.style = textstyle_normal();

    out->type = RNODE_CONTAINER;
    out->rect = rect_new(box_x, box_y, box_w, box_h);
    out->container.border = BORDER_SINGLE;
    out->container.padding = edgeinsets_zero();
    out->container.children = children;
    out->container.child_count = 5;
}

static EventResult text_editor_handle_event(Widget *self, Event *ev, Backend *backend) {
    (void)backend;
    TextEditorData *d = (TextEditorData *)(self + 1);
    if (ev->type != EVENT_KEY) return event_result_unhandled();

    switch (ev->code) {
        case KEY_ESC: {
            char *full = strdup("");
            for (int i = 0; i < d->line_count; i++) {
                char *t = malloc(strlen(full) + strlen(d->lines[i]) + 2);
                sprintf(t, "%s%s\n", full, d->lines[i]);
                free(full);
                full = t;
            }
            cJSON *result = cJSON_CreateString(full);
            free(full);
            return event_result_response((WidgetResponse){ .result = result, .cancelled = false, .error = NULL });
        }

        case KEY_UP:
            if (d->row > 0) d->row--;
            te_clamp_cursor(d);
            d->dirty = true;
            return event_result_handled();

        case KEY_DOWN:
            if (d->row + 1 < d->line_count) d->row++;
            te_clamp_cursor(d);
            d->dirty = true;
            return event_result_handled();

        case KEY_LEFT:
            if (d->col > 0) d->col--;
            else if (d->row > 0) { d->row--; d->col = strlen(d->lines[d->row]); }
            te_clamp_cursor(d);
            d->dirty = true;
            return event_result_handled();

        case KEY_RIGHT:
            if (d->col < (int)strlen(d->lines[d->row])) d->col++;
            else if (d->row + 1 < d->line_count) { d->row++; d->col = 0; }
            te_clamp_cursor(d);
            d->dirty = true;
            return event_result_handled();

        case KEY_HOME:
            d->col = 0;
            d->dirty = true;
            return event_result_handled();

        case KEY_END:
            d->col = strlen(d->lines[d->row]);
            d->dirty = true;
            return event_result_handled();

        case KEY_PAGEUP:
            d->row -= d->visible_h;
            if (d->row < 0) d->row = 0;
            te_clamp_cursor(d);
            d->dirty = true;
            return event_result_handled();

        case KEY_PAGEDOWN:
            d->row += d->visible_h;
            if (d->row >= d->line_count) d->row = d->line_count - 1;
            te_clamp_cursor(d);
            d->dirty = true;
            return event_result_handled();

        case KEY_DELETE:
            if (d->col < (int)strlen(d->lines[d->row])) {
                memmove(d->lines[d->row] + d->col, d->lines[d->row] + d->col + 1,
                        strlen(d->lines[d->row] + d->col));
                d->dirty = true;
            } else if (d->row + 1 < d->line_count) {
                int len = strlen(d->lines[d->row]);
                d->lines[d->row] = realloc(d->lines[d->row], len + strlen(d->lines[d->row + 1]) + 1);
                strcat(d->lines[d->row], d->lines[d->row + 1]);
                free(d->lines[d->row + 1]);
                memmove(&d->lines[d->row + 1], &d->lines[d->row + 2],
                        (d->line_count - d->row - 2) * sizeof(char *));
                d->line_count--;
                d->dirty = true;
            }
            return event_result_handled();

        case KEY_ENTER: {
            char *rest = strdup(d->lines[d->row] + d->col);
            d->lines[d->row][d->col] = '\0';
            d->line_count++;
            d->lines = realloc(d->lines, d->line_count * sizeof(char *));
            memmove(&d->lines[d->row + 2], &d->lines[d->row + 1],
                    (d->line_count - d->row - 2) * sizeof(char *));
            d->lines[d->row + 1] = rest;
            d->row++;
            d->col = 0;
            d->dirty = true;
            return event_result_handled();
        }

        case KEY_BACKSPACE:
            if (d->col > 0) {
                memmove(d->lines[d->row] + d->col - 1, d->lines[d->row] + d->col,
                        strlen(d->lines[d->row] + d->col) + 1);
                d->col--;
            } else if (d->row > 0) {
                d->col = strlen(d->lines[d->row - 1]);
                d->lines[d->row - 1] = realloc(d->lines[d->row - 1],
                        d->col + strlen(d->lines[d->row]) + 1);
                strcat(d->lines[d->row - 1], d->lines[d->row]);
                free(d->lines[d->row]);
                memmove(&d->lines[d->row], &d->lines[d->row + 1],
                        (d->line_count - d->row - 1) * sizeof(char *));
                d->line_count--;
                d->row--;
            }
            d->dirty = true;
            return event_result_handled();

        case KEY_CHAR: {
            if (ev->ch == 19) {
                te_save(d);
                d->dirty = true;
                return event_result_handled();
            }
            if (ev->ch == 4) {
                if (d->line_count > 1) {
                    free(d->lines[d->row]);
                    memmove(&d->lines[d->row], &d->lines[d->row + 1],
                            (d->line_count - d->row - 1) * sizeof(char *));
                    d->line_count--;
                    if (d->row >= d->line_count) d->row = d->line_count - 1;
                    d->col = 0;
                    d->dirty = true;
                }
                return event_result_handled();
            }
            if (ev->ch >= 32 || ev->ch == '\t') {
                int len = strlen(d->lines[d->row]);
                d->lines[d->row] = realloc(d->lines[d->row], len + 2);
                memmove(d->lines[d->row] + d->col + 1, d->lines[d->row] + d->col,
                        len - d->col + 1);
                d->lines[d->row][d->col] = ev->ch;
                d->col++;
                d->dirty = true;
            }
            return event_result_handled();
        }

        default:
            return event_result_unhandled();
    }
}

static bool te_is_dirty(Widget *self) { return ((TextEditorData *)(self + 1))->dirty; }
static void te_clear_dirty(Widget *self) { ((TextEditorData *)(self + 1))->dirty = false; }
static void te_destroy(Widget *self) {
    TextEditorData *d = (TextEditorData *)(self + 1);
    free(d->title);
    free(d->file_path);
    for (int i = 0; i < d->line_count; i++) free(d->lines[i]);
    free(d->lines);
}

Widget *text_editor_widget_new(const char *title, const char *file_path, const char *content) {
    Widget *w = calloc(1, sizeof(Widget) + sizeof(TextEditorData));
    w->vtable.render = text_editor_render;
    w->vtable.handle_event = text_editor_handle_event;
    w->vtable.is_dirty = te_is_dirty;
    w->vtable.clear_dirty = te_clear_dirty;
    w->vtable.destroy = te_destroy;
    TextEditorData *d = (TextEditorData *)(w + 1);
    d->title = strdup(title);
    d->file_path = file_path ? strdup(file_path) : NULL;
    d->lines = NULL;
    d->line_count = 0;

    if (file_path) {
        FILE *f = fopen(file_path, "r");
        if (f) {
            char buf[4096];
            while (fgets(buf, sizeof(buf), f)) {
                int len = strlen(buf);
                if (len > 0 && buf[len - 1] == '\n') buf[len - 1] = '\0';
                d->lines = realloc(d->lines, (d->line_count + 1) * sizeof(char *));
                d->lines[d->line_count++] = strdup(buf);
            }
            fclose(f);
        }
    }

    if (d->line_count == 0 && content) {
        te_split_content(d, content);
    }

    if (d->line_count == 0) {
        d->lines = malloc(sizeof(char *));
        d->lines[0] = strdup("");
        d->line_count = 1;
    }

    d->row = 0;
    d->col = 0;
    d->scroll = 0;
    d->visible_h = 20;
    d->dirty = true;
    return w;
}