#include "calendar.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

typedef struct {
    char *title;
    int year;
    int month;
    int selected_day;
    bool dirty;
} CalendarData;

static int days_in_month(int year, int month) {
    switch (month) {
        case 1: case 3: case 5: case 7: case 8: case 10: case 12: return 31;
        case 4: case 6: case 9: case 11: return 30;
        case 2: return (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)) ? 29 : 28;
        default: return 30;
    }
}

static void calendar_render(Widget *self, Rect area, RenderTree *out) {
    CalendarData *d = (CalendarData *)(self + 1);
    memset(out, 0, sizeof(*out));
    int box_w = 40, box_h = 12;
    if (box_w > area.w - 2) box_w = area.w - 2;
    int box_x = (area.w - box_w) / 2, box_y = (area.h - box_h) / 2;

    const char *months[] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
    RenderTree *children = calloc(3 + 1, sizeof(RenderTree));
    children[0].type = RNODE_TEXT;
    children[0].rect = rect_new(1, 0, box_w - 2, 1);
    char title_buf[64];
    snprintf(title_buf, sizeof(title_buf), "%s %s", d->title, months[d->month - 1]);
    children[0].text.content = strdup(title_buf);
    children[0].text.align = ALIGN_CENTER;
    children[0].text.style = textstyle_selected();

    children[1].type = RNODE_CALENDAR;
    children[1].rect = rect_new(1, 1, box_w - 2, box_h - 3);
    children[1].calendar.year = d->year;
    children[1].calendar.month = d->month;
    children[1].calendar.selected_day = d->selected_day;
    children[1].calendar.highlight = textstyle_selected();

    children[2].type = RNODE_TEXT;
    children[2].rect = rect_new(1, box_h - 2, box_w - 2, 1);
    children[2].text.content = strdup("Left/Right:day  Up/Down:week  Enter:select  Esc:cancel");
    children[2].text.align = ALIGN_CENTER;
    children[2].text.style = textstyle_muted();

    out->type = RNODE_CONTAINER;
    out->rect = rect_new(box_x, box_y, box_w, box_h);
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
            return event_result_response((WidgetResponse){ .result = NULL, .cancelled = true, .error = NULL });
        case KEY_LEFT: if (d->selected_day > 1) d->selected_day--; d->dirty = true; return event_result_handled();
        case KEY_RIGHT: if (d->selected_day < dim) d->selected_day++; d->dirty = true; return event_result_handled();
        case KEY_UP: d->selected_day = d->selected_day - 7 >= 1 ? d->selected_day - 7 : 1; d->dirty = true; return event_result_handled();
        case KEY_DOWN: d->selected_day = d->selected_day + 7 <= dim ? d->selected_day + 7 : dim; d->dirty = true; return event_result_handled();
        case KEY_ENTER: {
            char date[16];
            snprintf(date, sizeof(date), "%d-%02d-%02d", d->year, d->month, d->selected_day);
            return event_result_response((WidgetResponse){ .result = cJSON_CreateString(date), .cancelled = false, .error = NULL });
        }
        default: return event_result_unhandled();
    }
}

static bool calendar_is_dirty(Widget *self) { return ((CalendarData *)(self + 1))->dirty; }
static void calendar_clear_dirty(Widget *self) { ((CalendarData *)(self + 1))->dirty = false; }
static void calendar_destroy(Widget *self) { free(((CalendarData *)(self + 1))->title); }

Widget *calendar_widget_new(const char *title) {
    Widget *w = calloc(1, sizeof(Widget) + sizeof(CalendarData));
    w->vtable.render = calendar_render;
    w->vtable.handle_event = calendar_handle_event;
    w->vtable.is_dirty = calendar_is_dirty;
    w->vtable.clear_dirty = calendar_clear_dirty;
    w->vtable.destroy = calendar_destroy;
    CalendarData *d = (CalendarData *)(w + 1);
    d->title = strdup(title);
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    d->year = tm->tm_year + 1900;
    d->month = tm->tm_mon + 1;
    d->selected_day = tm->tm_mday;
    d->dirty = true;
    return w;
}