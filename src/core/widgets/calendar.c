#include "calendar.h"
#include "core/widget_base.h"
#include "core/session.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

typedef struct { WidgetBase base; char *title; int year, month, selected_day; } CalendarData;
extern Arena *g_session_arena;

static int days_in_month(int y, int m) {
    switch (m) {
        case 1: case 3: case 5: case 7: case 8: case 10: case 12: return 31;
        case 4: case 6: case 9: case 11: return 30;
        case 2: return (y % 4 == 0 && (y % 100 != 0 || y % 400 == 0)) ? 29 : 28;
        default: return 30;
    }
}

static void calendar_render(Widget *self, Rect area, RenderTree *out) {
    CalendarData *d = (CalendarData *)(self + 1);
    memset(out, 0, sizeof(*out));
    out->style_class = "container";
    int box_w = 40, box_h = 12;
    if (box_w > area.w - 2) box_w = area.w - 2;
    if (box_h > area.h - 2) box_h = area.h - 2;
    RenderTree *children = arena_alloc(g_session_arena, 3 * sizeof(RenderTree));
    children[0].type = RNODE_TEXT;
    children[0].rect = rect_new(1, 0, box_w - 2, 1);
    const char *months[] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
    char title_buf[64];
    snprintf(title_buf, sizeof(title_buf), "%s %s", d->title, months[d->month - 1]);
    children[0].text.content = arena_strdup(g_session_arena, title_buf);
    children[0].style_class = "text";
    children[0].state = "title";
    children[1].type = RNODE_CALENDAR;
    children[1].rect = rect_new(1, 1, box_w - 2, box_h - 3);
    children[1].calendar.year = d->year;
    children[1].calendar.month = d->month;
    children[1].calendar.selected_day = d->selected_day;
    children[1].style_class = "calendar";
    children[2].type = RNODE_TEXT;
    children[2].rect = rect_new(1, box_h - 2, box_w - 2, 1);
    children[2].text.content = "Left/Right:day  Up/Down:week  Enter:select  Esc:cancel";
    children[2].style_class = "text";
    children[2].state = "muted";
    out->type = RNODE_CONTAINER;
    out->rect = rect_new((area.w - box_w) / 2, (area.h - box_h) / 2, box_w, box_h);
    out->container.border = BORDER_SINGLE;
    out->container.padding = edgeinsets_zero();
    out->container.children = children;
    out->container.child_count = 3;
}

static EventResult calendar_handle_event(Widget *self, Event *ev, Backend *backend) {
    (void)backend;
    CalendarData *d = (CalendarData *)(self + 1);
    if (ev->type != EVENT_KEY) return event_result_unhandled();
    int dim = days_in_month(d->year, d->month);
    switch (ev->code) {
        case KEY_ESC:
            return event_result_response((WidgetResponse){ .result = NULL, .cancelled = true });
        case KEY_LEFT:
            if (d->selected_day > 1) d->selected_day--;
            d->base.dirty = true;
            return event_result_handled();
        case KEY_RIGHT:
            if (d->selected_day < dim) d->selected_day++;
            d->base.dirty = true;
            return event_result_handled();
        case KEY_UP:
            d->selected_day = d->selected_day - 7 >= 1 ? d->selected_day - 7 : 1;
            d->base.dirty = true;
            return event_result_handled();
        case KEY_DOWN:
            d->selected_day = d->selected_day + 7 <= dim ? d->selected_day + 7 : dim;
            d->base.dirty = true;
            return event_result_handled();
        case KEY_ENTER: {
            char date[16];
            snprintf(date, sizeof(date), "%d-%02d-%02d", d->year, d->month, d->selected_day);
            return event_result_response((WidgetResponse){ .result = cJSON_CreateString(date), .cancelled = false });
        }
        default:
            return event_result_unhandled();
    }
}

static void calendar_destroy(Widget *self) {
    free(((CalendarData *)(self + 1))->title);
}

Widget *calendar_widget_new(const char *title) {
    Widget *w = calloc(1, sizeof(Widget) + sizeof(CalendarData));
    CalendarData *d = (CalendarData *)(w + 1);
    d->base.dirty = true;
    d->title = strdup(title);
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    d->year = tm->tm_year + 1900;
    d->month = tm->tm_mon + 1;
    d->selected_day = tm->tm_mday;
    w->vtable.render = calendar_render;
    w->vtable.handle_event = calendar_handle_event;
    w->vtable.is_dirty = widget_base_is_dirty;
    w->vtable.clear_dirty = widget_base_clear_dirty;
    w->vtable.destroy = calendar_destroy;
    return w;
}