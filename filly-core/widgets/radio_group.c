#include <stdio.h>
#include "radio_group.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *title;
    char *message;
    char **choices;
    int count;
    int selected;
    bool dirty;
} RadioGroupData;

static void radio_group_render(Widget *self, Rect area, RenderTree *out) {
    RadioGroupData *d = (RadioGroupData *)(self + 1);
    memset(out, 0, sizeof(*out));
    int box_w = (int)(area.w * 0.5f); if (box_w > area.w - 2) box_w = area.w - 2;
    int box_h = (int)(area.h * 0.6f); if (box_h > area.h - 2) box_h = area.h - 2;
    int box_x = (area.w - box_w) / 2, box_y = (area.h - box_h) / 2;

    out->accessible.role = strdup("dialog");
    out->accessible.label = strdup(d->title ? d->title : "Radio Group");

    RenderTree *children = calloc(3 + 1, sizeof(RenderTree));
    int idx = 0;
    if (d->title && strlen(d->title)) {
        children[idx].type = RNODE_TEXT; children[idx].rect = rect_new(1, 0, box_w - 2, 1);
        children[idx].text.content = strdup(d->title); children[idx].text.align = ALIGN_CENTER;
        children[idx].text.style = textstyle_selected();
        children[idx].accessible.role = strdup("heading"); children[idx].accessible.label = strdup(d->title);
        idx++;
    }
    int list_y = 1;
    if (d->message && strlen(d->message)) {
        children[idx].type = RNODE_TEXT; children[idx].rect = rect_new(1, 1, box_w - 2, 2);
        children[idx].text.content = strdup(d->message); children[idx].text.align = ALIGN_LEFT;
        children[idx].text.style = textstyle_normal(); children[idx].accessible.role = strdup("description");
        idx++; list_y = 3;
    }
    children[idx].type = RNODE_LIST; children[idx].rect = rect_new(1, list_y, box_w - 2, box_h - list_y - 2);
    children[idx].list.item_count = d->count; children[idx].list.items = malloc(d->count * sizeof(ListItem));
    for (int i = 0; i < d->count; i++) { char label[512]; snprintf(label, sizeof(label), " %s %s", i == d->selected ? "(●)" : "( )", d->choices[i]); children[idx].list.items[i] = listitem_new(label); }
    children[idx].list.selected = d->selected; children[idx].list.highlight = textstyle_selected();
    children[idx].accessible.role = strdup("radiogroup");
    idx++;

    children[idx].type = RNODE_TEXT; children[idx].rect = rect_new(1, box_h - 2, box_w - 2, 1);
    children[idx].text.content = strdup("Up/Down:move  Enter:select  Esc:cancel");
    children[idx].text.align = ALIGN_CENTER; children[idx].text.style = textstyle_muted();
    idx++;

    out->type = RNODE_CONTAINER;
    out->rect = rect_new(box_x, box_y, box_w, box_h);
    out->container.border = BORDER_SINGLE; out->container.padding = edgeinsets_zero();
    out->container.children = children; out->container.child_count = idx;
}

static EventResult radio_group_handle_event(Widget *self, Event *ev, Backend *backend) {
    (void)backend;
    RadioGroupData *d = (RadioGroupData *)(self + 1);
    if (ev->type != EVENT_KEY) return event_result_unhandled();
    switch (ev->code) {
        case KEY_ESC: return event_result_response((WidgetResponse){ .result = NULL, .cancelled = true, .error = NULL });
        case KEY_UP: if (d->selected > 0) { d->selected--; d->dirty = true; } return event_result_handled();
        case KEY_DOWN: if (d->selected + 1 < d->count) { d->selected++; d->dirty = true; } return event_result_handled();
        case KEY_ENTER: return event_result_response((WidgetResponse){ .result = cJSON_CreateString(d->choices[d->selected]), .cancelled = false, .error = NULL });
        default: return event_result_unhandled();
    }
}

static bool radio_is_dirty(Widget *self) { return ((RadioGroupData *)(self + 1))->dirty; }
static void radio_clear_dirty(Widget *self) { ((RadioGroupData *)(self + 1))->dirty = false; }
static void radio_destroy(Widget *self) { RadioGroupData *d = (RadioGroupData *)(self + 1); free(d->title); free(d->message); for (int i = 0; i < d->count; i++) free(d->choices[i]); free(d->choices); }

Widget *radio_group_widget_new(const char *title, const char *message, char **choices, int count, int default_idx) {
    Widget *w = calloc(1, sizeof(Widget) + sizeof(RadioGroupData));
    w->vtable.render = radio_group_render; w->vtable.handle_event = radio_group_handle_event;
    w->vtable.is_dirty = radio_is_dirty; w->vtable.clear_dirty = radio_clear_dirty; w->vtable.destroy = radio_destroy;
    RadioGroupData *d = (RadioGroupData *)(w + 1); d->title = title ? strdup(title) : NULL; d->message = message ? strdup(message) : NULL;
    d->choices = malloc(count * sizeof(char *)); for (int i = 0; i < count; i++) d->choices[i] = strdup(choices[i]);
    d->count = count; d->selected = default_idx >= 0 && default_idx < count ? default_idx : 0; d->dirty = true;
    return w;
}