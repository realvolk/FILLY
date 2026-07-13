#include <stdio.h>
#include "table.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *title;
    char **headers;
    int header_count;
    char ***rows;
    int row_count;
    int selected_row;
    int selected_col;
    bool dirty;
} TableData;

static void table_render(Widget *self, Rect area, RenderTree *out) {
    TableData *d = (TableData *)(self + 1);
    memset(out, 0, sizeof(*out));
    int box_w = (int)(area.w * 0.9f);
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

    children[1].type = RNODE_TABLE;
    children[1].rect = rect_new(1, 1, box_w - 2, box_h - 3);
    children[1].table.headers = malloc(d->header_count * sizeof(char *));
    children[1].table.header_count = d->header_count;
    for (int i = 0; i < d->header_count; i++) children[1].table.headers[i] = strdup(d->headers[i]);
    children[1].table.rows = malloc(d->row_count * sizeof(char **));
    children[1].table.row_count = d->row_count;
    for (int i = 0; i < d->row_count; i++) {
        children[1].table.rows[i] = malloc(d->header_count * sizeof(char *));
        for (int j = 0; j < d->header_count; j++)
            children[1].table.rows[i][j] = strdup(d->rows[i][j]);
    }
    children[1].table.selected_row = d->selected_row;
    children[1].table.selected_col = d->selected_col;
    children[1].table.highlight = textstyle_selected();

    children[2].type = RNODE_TEXT;
    children[2].rect = rect_new(1, box_h - 2, box_w - 2, 1);
    children[2].text.content = strdup("Up/Down:row  Left/Right:col  Enter:select  Esc:cancel");
    children[2].text.align = ALIGN_CENTER;
    children[2].text.style = textstyle_muted();

    out->type = RNODE_CONTAINER;
    out->rect = rect_new(box_x, box_y, box_w, box_h);
    out->container.border = BORDER_SINGLE;
    out->container.padding = edgeinsets_zero();
    out->container.children = children;
    out->container.child_count = 3;
}

static EventResult table_handle_event(Widget *self, Event *ev, Backend *backend) {
    TableData *d = (TableData *)(self + 1);
    if (ev->type != EVENT_KEY) return event_result_unhandled();
    switch (ev->code) {
        case KEY_ESC:
            return event_result_response((WidgetResponse){ .result = NULL, .cancelled = true, .error = NULL });
        case KEY_UP: if (d->selected_row > 0) d->selected_row--; d->dirty = true; return event_result_handled();
        case KEY_DOWN: if (d->selected_row + 1 < d->row_count) d->selected_row++; d->dirty = true; return event_result_handled();
        case KEY_LEFT: if (d->selected_col > 0) d->selected_col--; d->dirty = true; return event_result_handled();
        case KEY_RIGHT: if (d->selected_col + 1 < d->header_count) d->selected_col++; d->dirty = true; return event_result_handled();
        case KEY_ENTER:
            if (d->selected_row < d->row_count && d->selected_col < d->header_count)
                return event_result_response((WidgetResponse){ .result = cJSON_CreateString(d->rows[d->selected_row][d->selected_col]), .cancelled = false, .error = NULL });
            return event_result_handled();
        default: return event_result_unhandled();
    }
}

static bool table_is_dirty(Widget *self) { return ((TableData *)(self + 1))->dirty; }
static void table_clear_dirty(Widget *self) { ((TableData *)(self + 1))->dirty = false; }
static void table_destroy(Widget *self) {
    TableData *d = (TableData *)(self + 1);
    free(d->title);
    for (int i = 0; i < d->header_count; i++) free(d->headers[i]);
    free(d->headers);
    for (int i = 0; i < d->row_count; i++) {
        for (int j = 0; j < d->header_count; j++) free(d->rows[i][j]);
        free(d->rows[i]);
    }
    free(d->rows);
}

Widget *table_widget_new(const char *title, char **headers, int header_count, char ***rows, int row_count) {
    Widget *w = calloc(1, sizeof(Widget) + sizeof(TableData));
    w->vtable.render = table_render;
    w->vtable.handle_event = table_handle_event;
    w->vtable.is_dirty = table_is_dirty;
    w->vtable.clear_dirty = table_clear_dirty;
    w->vtable.destroy = table_destroy;
    TableData *d = (TableData *)(w + 1);
    d->title = strdup(title);
    d->headers = malloc(header_count * sizeof(char *));
    d->header_count = header_count;
    for (int i = 0; i < header_count; i++) d->headers[i] = strdup(headers[i]);
    d->rows = malloc(row_count * sizeof(char **));
    d->row_count = row_count;
    for (int i = 0; i < row_count; i++) {
        d->rows[i] = malloc(header_count * sizeof(char *));
        for (int j = 0; j < header_count; j++) d->rows[i][j] = strdup(rows[i][j]);
    }
    d->selected_row = 0; d->selected_col = 0;
    d->dirty = true;
    return w;
}