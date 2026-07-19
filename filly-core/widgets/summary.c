#include "summary.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct {
    char *title;
    char *text;
    int scroll;
    bool dirty;
} SummaryData;

static void summary_render(Widget *self, Rect area, RenderTree *out) {
    SummaryData *d = (SummaryData *)(self + 1);
    memset(out, 0, sizeof(*out));
    int box_w = (int)(area.w * 0.8f);
    if (box_w > area.w - 2) box_w = area.w - 2;
    int box_h = (int)(area.h * 0.8f);
    if (box_h > area.h - 2) box_h = area.h - 2;
    int box_x = (area.w - box_w) / 2, box_y = (area.h - box_h) / 2;

    out->accessible.role = strdup("dialog");
    out->accessible.label = strdup(d->title ? d->title : "Summary");

    RenderTree *children = calloc(3 + 1, sizeof(RenderTree));
    children[0].type = RNODE_TEXT;
    children[0].rect = rect_new(1, 0, box_w - 2, 1);
    children[0].text.content = strdup(d->title);
    children[0].text.align = ALIGN_CENTER;
    children[0].text.style = textstyle_selected();
    children[0].accessible.role = strdup("heading");
    children[0].accessible.label = strdup(d->title);

    children[1].type = RNODE_TEXT;
    children[1].rect = rect_new(1, 1, box_w - 2, box_h - 3);
    children[1].text.content = strdup(d->text);
    children[1].text.align = ALIGN_LEFT;
    children[1].text.style = textstyle_normal();

    children[2].type = RNODE_TEXT;
    children[2].rect = rect_new(1, box_h - 2, box_w - 2, 1);
    children[2].text.content = strdup("Any key to continue  Esc:cancel");
    children[2].text.align = ALIGN_CENTER;
    children[2].text.style = textstyle_muted();

    out->type = RNODE_CONTAINER;
    out->rect = rect_new(box_x, box_y, box_w, box_h);
    out->container.border = BORDER_SINGLE;
    out->container.padding = edgeinsets_zero();
    out->container.children = children;
    out->container.child_count = 3;
}

static EventResult summary_handle_event(Widget *self, Event *ev, Backend *backend) {
    (void)self; (void)backend;
    if (ev->type == EVENT_KEY) {
        if (ev->code == KEY_ESC)
            return event_result_response((WidgetResponse){ .result = NULL, .cancelled = true, .error = NULL });
        return event_result_response((WidgetResponse){ .result = NULL, .cancelled = false, .error = NULL });
    }
    return event_result_unhandled();
}

static bool summary_is_dirty(Widget *self) { return ((SummaryData *)(self + 1))->dirty; }
static void summary_clear_dirty(Widget *self) { ((SummaryData *)(self + 1))->dirty = false; }
static void summary_destroy(Widget *self) {
    SummaryData *d = (SummaryData *)(self + 1);
    free(d->title); free(d->text);
}

Widget *summary_widget_new(const char *title, const char *message, const char *file_path) {
    Widget *w = calloc(1, sizeof(Widget) + sizeof(SummaryData));
    w->vtable.render = summary_render;
    w->vtable.handle_event = summary_handle_event;
    w->vtable.is_dirty = summary_is_dirty;
    w->vtable.clear_dirty = summary_clear_dirty;
    w->vtable.destroy = summary_destroy;
    SummaryData *d = (SummaryData *)(w + 1);
    d->title = strdup(title);
    if (file_path) {
        FILE *f = fopen(file_path, "r");
        if (f) { fseek(f, 0, SEEK_END); long sz = ftell(f); rewind(f); d->text = malloc(sz + 1); fread(d->text, 1, sz, f); d->text[sz] = '\0'; fclose(f); }
        else d->text = strdup("[Error reading file]");
    } else d->text = message ? strdup(message) : strdup("");
    d->scroll = 0; d->dirty = true;
    return w;
}