#include "text_editor.h"
#include "core/widget_base.h"
#include "core/session.h"
#include "core/undo.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct { WidgetBase base; char *title, *file_path, **lines; int line_count, row, col, scroll, visible_h; } TextEditorData;
extern Arena *g_session_arena;

static void te_clamp_cursor(TextEditorData *d) {
    if (d->line_count == 0) { d->lines = realloc(d->lines, sizeof(char *)); d->lines[0] = strdup(""); d->line_count = 1; }
    if (d->row >= d->line_count) d->row = d->line_count - 1;
    if (d->row < 0) d->row = 0;
    if (d->col > (int)strlen(d->lines[d->row])) d->col = strlen(d->lines[d->row]);
    if (d->col < 0) d->col = 0;
    if (d->row < d->scroll) d->scroll = d->row;
    if (d->row >= d->scroll + d->visible_h) d->scroll = d->row - d->visible_h + 1;
    if (d->scroll < 0) d->scroll = 0;
}

static void te_save(TextEditorData *d) { if (!d->file_path) return; FILE *f = fopen(d->file_path, "w"); if (!f) return; for (int i = 0; i < d->line_count; i++) fprintf(f, "%s\n", d->lines[i]); fclose(f); }

static void text_editor_render(Widget *self, Rect area, RenderTree *out) {
    TextEditorData *d = (TextEditorData *)(self + 1);
    d->visible_h = (int)(area.h * 0.9f) - 4; if (d->visible_h < 1) d->visible_h = 1;
    te_clamp_cursor(d);
    memset(out, 0, sizeof(*out)); out->style_class = "container";
    int box_w = (int)(area.w * 0.9f); if (box_w > area.w - 2) box_w = area.w - 2;
    int box_h = (int)(area.h * 0.9f); if (box_h > area.h - 2) box_h = area.h - 2;
    RenderTree *children = arena_alloc(g_session_arena, 5 * sizeof(RenderTree));
    children[0].type = RNODE_TEXT; children[0].rect = rect_new(1, 0, box_w - 2, 1);
    char title_buf[512]; snprintf(title_buf, sizeof(title_buf), "%s - %s  Ln %d, Col %d", d->title, d->file_path ? d->file_path : "", d->row+1, d->col+1);
    children[0].text.content = arena_strdup(g_session_arena, title_buf); children[0].style_class = "text"; children[0].state = "title";
    char *display = arena_alloc(g_session_arena, d->visible_h * (box_w + 2)); display[0] = '\0';
    int end = d->scroll + d->visible_h; if (end > d->line_count) end = d->line_count;
    for (int i = d->scroll; i < end; i++) { strcat(display, d->lines[i]); if (i < end - 1) strcat(display, "\n"); }
    children[1].type = RNODE_TEXT; children[1].rect = rect_new(1, 1, box_w - 2, d->visible_h); children[1].text.content = display; children[1].style_class = "text";
    children[2].type = RNODE_TEXT; children[2].rect = rect_new(1, box_h - 3, box_w - 2, 1); children[2].text.content = "^S:save  ^D:delete line  Esc:done  Arrows:move"; children[2].style_class = "text"; children[2].state = "muted";
    children[3].type = RNODE_CURSOR; children[3].rect = rect_new(1, 1, 1, 1); children[3].cursor.x = d->col; children[3].cursor.y = d->row - d->scroll;
    children[4].type = RNODE_TEXT; children[4].rect = rect_new(1, box_h - 2, box_w - 2, 1); children[4].text.content = "";
    out->type = RNODE_CONTAINER; out->rect = rect_new((area.w - box_w) / 2, (area.h - box_h) / 2, box_w, box_h);
    out->container.border = BORDER_SINGLE; out->container.padding = edgeinsets_zero();
    out->container.children = children; out->container.child_count = 5;
}

static EventResult text_editor_handle_event(Widget *self, Event *ev, Backend *backend) {
    (void)backend;
    TextEditorData *d = (TextEditorData *)(self + 1);
    if (ev->type != EVENT_KEY) return event_result_unhandled();
    switch (ev->code) {
        case KEY_ESC: { char *full = strdup(""); for (int i = 0; i < d->line_count; i++) { char *t = malloc(strlen(full) + strlen(d->lines[i]) + 2); sprintf(t, "%s%s\n", full, d->lines[i]); free(full); full = t; } cJSON *r = cJSON_CreateString(full); free(full); return event_result_response((WidgetResponse){ .result = r, .cancelled = false }); }
        case KEY_UP: if (d->row > 0) d->row--; te_clamp_cursor(d); d->base.dirty = true; return event_result_handled();
        case KEY_DOWN: if (d->row + 1 < d->line_count) d->row++; te_clamp_cursor(d); d->base.dirty = true; return event_result_handled();
        case KEY_LEFT: if (d->col > 0) d->col--; else if (d->row > 0) { d->row--; d->col = strlen(d->lines[d->row]); } te_clamp_cursor(d); d->base.dirty = true; return event_result_handled();
        case KEY_RIGHT: if (d->col < (int)strlen(d->lines[d->row])) d->col++; else if (d->row + 1 < d->line_count) { d->row++; d->col = 0; } te_clamp_cursor(d); d->base.dirty = true; return event_result_handled();
        case KEY_HOME: d->col = 0; d->base.dirty = true; return event_result_handled();
        case KEY_END: d->col = strlen(d->lines[d->row]); d->base.dirty = true; return event_result_handled();
        case KEY_PAGEUP: d->row -= d->visible_h; if (d->row < 0) d->row = 0; te_clamp_cursor(d); d->base.dirty = true; return event_result_handled();
        case KEY_PAGEDOWN: d->row += d->visible_h; if (d->row >= d->line_count) d->row = d->line_count - 1; te_clamp_cursor(d); d->base.dirty = true; return event_result_handled();
        case KEY_DELETE: if (d->col < (int)strlen(d->lines[d->row])) { memmove(d->lines[d->row]+d->col, d->lines[d->row]+d->col+1, strlen(d->lines[d->row]+d->col)); d->base.dirty = true; } else if (d->row+1 < d->line_count) { d->lines[d->row] = realloc(d->lines[d->row], strlen(d->lines[d->row])+strlen(d->lines[d->row+1])+1); strcat(d->lines[d->row], d->lines[d->row+1]); free(d->lines[d->row+1]); memmove(&d->lines[d->row+1], &d->lines[d->row+2], (d->line_count-d->row-2)*sizeof(char*)); d->line_count--; d->base.dirty = true; } return event_result_handled();
        case KEY_ENTER: { char *rest = strdup(d->lines[d->row]+d->col); d->lines[d->row][d->col] = '\0'; d->line_count++; d->lines = realloc(d->lines, d->line_count*sizeof(char*)); memmove(&d->lines[d->row+2], &d->lines[d->row+1], (d->line_count-d->row-2)*sizeof(char*)); d->lines[d->row+1] = rest; d->row++; d->col = 0; d->base.dirty = true; return event_result_handled(); }
        case KEY_BACKSPACE: if (d->col > 0) { memmove(d->lines[d->row]+d->col-1, d->lines[d->row]+d->col, strlen(d->lines[d->row]+d->col)+1); d->col--; } else if (d->row > 0) { d->col = strlen(d->lines[d->row-1]); d->lines[d->row-1] = realloc(d->lines[d->row-1], d->col+strlen(d->lines[d->row])+1); strcat(d->lines[d->row-1], d->lines[d->row]); free(d->lines[d->row]); memmove(&d->lines[d->row], &d->lines[d->row+1], (d->line_count-d->row-1)*sizeof(char*)); d->line_count--; d->row--; } d->base.dirty = true; return event_result_handled();
        case KEY_CHAR: if (ev->ch == 19) { te_save(d); d->base.dirty = true; return event_result_handled(); } if (ev->ch == 4) { if (d->line_count > 1) { free(d->lines[d->row]); memmove(&d->lines[d->row], &d->lines[d->row+1], (d->line_count-d->row-1)*sizeof(char*)); d->line_count--; if (d->row >= d->line_count) d->row = d->line_count-1; d->col = 0; d->base.dirty = true; } return event_result_handled(); } if (ev->ch >= 32 || ev->ch == '\t') { int len = strlen(d->lines[d->row]); d->lines[d->row] = realloc(d->lines[d->row], len+2); memmove(d->lines[d->row]+d->col+1, d->lines[d->row]+d->col, len-d->col+1); d->lines[d->row][d->col] = ev->ch; d->col++; d->base.dirty = true; } return event_result_handled();
        default: return event_result_unhandled();
    }
}

static void te_destroy(Widget *self) { TextEditorData *d = (TextEditorData *)(self + 1); free(d->title); free(d->file_path); for (int i = 0; i < d->line_count; i++) free(d->lines[i]); free(d->lines); }

Widget *text_editor_widget_new(const char *title, const char *file_path, const char *content) {
    Widget *w = calloc(1, sizeof(Widget) + sizeof(TextEditorData));
    TextEditorData *d = (TextEditorData *)(w + 1);
    d->base.dirty = true;
    d->base.accepts_text_input = true;
    d->title = strdup(title);
    d->file_path = file_path ? strdup(file_path) : NULL;
    d->lines = NULL; d->line_count = 0; d->row = 0; d->col = 0; d->scroll = 0; d->visible_h = 20;
    if (file_path) { FILE *f = fopen(file_path, "r"); if (f) { char buf[4096]; while (fgets(buf, sizeof(buf), f)) { int len = strlen(buf); if (len>0 && buf[len-1]=='\n') buf[len-1]='\0'; d->lines = realloc(d->lines, (d->line_count+1)*sizeof(char*)); d->lines[d->line_count++] = strdup(buf); } fclose(f); } }
    if (d->line_count == 0 && content) { const char *p = content; const char *s = p; while (*p) { if (*p == '\n') { int len = p-s; d->lines = realloc(d->lines, (d->line_count+1)*sizeof(char*)); d->lines[d->line_count] = malloc(len+1); memcpy(d->lines[d->line_count], s, len); d->lines[d->line_count][len]='\0'; d->line_count++; s = p+1; } p++; } if (p > s || d->line_count == 0) { int len = p-s; d->lines = realloc(d->lines, (d->line_count+1)*sizeof(char*)); d->lines[d->line_count] = malloc(len+1); memcpy(d->lines[d->line_count], s, len); d->lines[d->line_count][len]='\0'; d->line_count++; } }
    if (d->line_count == 0) { d->lines = malloc(sizeof(char*)); d->lines[0] = strdup(""); d->line_count = 1; }
    d->row = 0; d->col = strlen(d->lines[0]);
    w->vtable.render = text_editor_render;
    w->vtable.handle_event = text_editor_handle_event;
    w->vtable.is_dirty = widget_base_is_dirty;
    w->vtable.clear_dirty = widget_base_clear_dirty;
    w->vtable.destroy = te_destroy;
    return w;
}