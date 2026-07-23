#include "table.h"
#include "core/widget_base.h"
#include "core/session.h"
#include <stdlib.h>
#include <string.h>

typedef struct { WidgetBase base; char *title, **headers, ***rows; int header_count, row_count, selected_row, selected_col; } TableData;
extern Arena *g_session_arena;

static void table_render(Widget *self, Rect area, RenderTree *out) {
    TableData *d = (TableData *)(self + 1);
    memset(out, 0, sizeof(*out)); out->style_class = "container";
    int box_w = (int)(area.w * 0.9f); if (box_w > area.w - 2) box_w = area.w - 2;
    int box_h = (int)(area.h * 0.8f); if (box_h > area.h - 2) box_h = area.h - 2;
    RenderTree *children = arena_alloc(g_session_arena, 3 * sizeof(RenderTree));
    children[0].type = RNODE_TEXT; children[0].rect = rect_new(1, 0, box_w - 2, 1);
    children[0].text.content = arena_strdup(g_session_arena, d->title); children[0].style_class = "text"; children[0].state = "title";
    children[1].type = RNODE_TABLE; children[1].rect = rect_new(1, 1, box_w - 2, box_h - 3);
    children[1].table.header_count = d->header_count; children[1].table.row_count = d->row_count;
    children[1].table.selected_row = d->selected_row; children[1].table.selected_col = d->selected_col;
    children[1].table.headers = arena_alloc(g_session_arena, d->header_count * sizeof(char *));
    for (int i = 0; i < d->header_count; i++) children[1].table.headers[i] = arena_strdup(g_session_arena, d->headers[i]);
    children[1].table.rows = arena_alloc(g_session_arena, d->row_count * sizeof(char **));
    for (int i = 0; i < d->row_count; i++) { children[1].table.rows[i] = arena_alloc(g_session_arena, d->header_count * sizeof(char *)); for (int j = 0; j < d->header_count; j++) children[1].table.rows[i][j] = arena_strdup(g_session_arena, d->rows[i][j]); }
    children[1].style_class = "table";
    children[2].type = RNODE_TEXT; children[2].rect = rect_new(1, box_h - 2, box_w - 2, 1);
    children[2].text.content = "Up/Down:row  Left/Right:col  Enter:select  Esc:cancel"; children[2].style_class = "text"; children[2].state = "muted";
    out->type = RNODE_CONTAINER; out->rect = rect_new((area.w - box_w) / 2, (area.h - box_h) / 2, box_w, box_h);
    out->container.border = BORDER_SINGLE; out->container.padding = edgeinsets_zero();
    out->container.children = children; out->container.child_count = 3;
}

static EventResult table_handle_event(Widget *self, Event *ev, Backend *backend) {
    (void)backend; TableData *d = (TableData *)(self + 1);
    if (ev->type != EVENT_KEY) return event_result_unhandled();
    switch (ev->code) {
        case KEY_ESC: return event_result_response((WidgetResponse){ .result = NULL, .cancelled = true });
        case KEY_UP: if (d->selected_row > 0) d->selected_row--; d->base.dirty = true; return event_result_handled();
        case KEY_DOWN: if (d->selected_row + 1 < d->row_count) d->selected_row++; d->base.dirty = true; return event_result_handled();
        case KEY_LEFT: if (d->selected_col > 0) d->selected_col--; d->base.dirty = true; return event_result_handled();
        case KEY_RIGHT: if (d->selected_col + 1 < d->header_count) d->selected_col++; d->base.dirty = true; return event_result_handled();
        case KEY_ENTER: if (d->selected_row < d->row_count && d->selected_col < d->header_count) return event_result_response((WidgetResponse){ .result = cJSON_CreateString(d->rows[d->selected_row][d->selected_col]), .cancelled = false }); return event_result_handled();
        default: return event_result_unhandled();
    }
}

static void table_destroy(Widget *self) { TableData *d = (TableData *)(self + 1); free(d->title); for (int i=0;i<d->header_count;i++) free(d->headers[i]); free(d->headers); for (int i=0;i<d->row_count;i++){for(int j=0;j<d->header_count;j++) free(d->rows[i][j]); free(d->rows[i]);} free(d->rows); }

Widget *table_widget_new(const char *title, char **headers, int header_count, char ***rows, int row_count) {
    Widget *w = calloc(1, sizeof(Widget) + sizeof(TableData));
    TableData data = { .title = strdup(title), .header_count = header_count, .row_count = row_count, .selected_row = 0, .selected_col = 0 };
    data.headers = malloc(header_count * sizeof(char *)); for (int i=0;i<header_count;i++) data.headers[i]=strdup(headers[i]);
    data.rows = malloc(row_count * sizeof(char **)); for (int i=0;i<row_count;i++){ data.rows[i]=malloc(header_count*sizeof(char*)); for(int j=0;j<header_count;j++) data.rows[i][j]=strdup(rows[i][j]); }
    widget_base_init(w, &data, sizeof(TableData), table_render, table_handle_event, table_destroy);
    return w;
}