#include "form.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct {
    char *title;
    FormField *fields;
    int field_count;
    int focused;
    char *submit_label;
    bool dirty;
} FormData;

static void form_render(Widget *self, Rect area, RenderTree *out) {
    FormData *d = (FormData *)(self + 1);
    memset(out, 0, sizeof(*out));
    int box_w = 60, box_h = 4 + d->field_count * 2 + 2;
    if (box_w > area.w - 2) box_w = area.w - 2;
    if (box_h > area.h - 2) box_h = area.h - 2;
    int box_x = (area.w - box_w) / 2, box_y = (area.h - box_h) / 2;

    out->accessible.role = strdup("dialog");
    out->accessible.label = strdup(d->title ? d->title : "Form");

    RenderTree *children = calloc(3 + 1, sizeof(RenderTree));
    children[0].type = RNODE_TEXT; children[0].rect = rect_new(1, 0, box_w - 2, 1);
    children[0].text.content = strdup(d->title); children[0].text.align = ALIGN_CENTER;
    children[0].text.style = textstyle_selected();
    children[0].accessible.role = strdup("heading"); children[0].accessible.label = strdup(d->title);

    children[1].type = RNODE_FORM; children[1].rect = rect_new(1, 1, box_w - 2, box_h - 3);
    children[1].form.fields = malloc(d->field_count * sizeof(FormField)); children[1].form.field_count = d->field_count;
    for (int i = 0; i < d->field_count; i++) {
        children[1].form.fields[i].label = strdup(d->fields[i].label);
        children[1].form.fields[i].widget_type = strdup(d->fields[i].widget_type);
        children[1].form.fields[i].value = strdup(d->fields[i].value);
        children[1].form.fields[i].choices = d->fields[i].choices ? malloc(d->fields[i].choice_count * sizeof(char *)) : NULL;
        children[1].form.fields[i].choice_count = d->fields[i].choice_count;
        for (int j = 0; j < d->fields[i].choice_count; j++) children[1].form.fields[i].choices[j] = strdup(d->fields[i].choices[j]);
        children[1].form.fields[i].placeholder = strdup(d->fields[i].placeholder);
    }
    children[1].form.focused = d->focused; children[1].form.submit_label = strdup(d->submit_label);
    children[1].accessible.role = strdup("form");

    children[2].type = RNODE_TEXT; children[2].rect = rect_new(1, box_h - 2, box_w - 2, 1);
    children[2].text.content = strdup("Tab:next  Enter:submit  Esc:cancel");
    children[2].text.align = ALIGN_CENTER; children[2].text.style = textstyle_muted();

    out->type = RNODE_CONTAINER;
    out->rect = rect_new(box_x, box_y, box_w, box_h);
    out->container.border = BORDER_SINGLE; out->container.padding = edgeinsets_zero();
    out->container.children = children; out->container.child_count = 3;
}

static EventResult form_handle_event(Widget *self, Event *ev, Backend *backend) {
    (void)backend;
    FormData *d = (FormData *)(self + 1);
    if (ev->type != EVENT_KEY) return event_result_unhandled();
    switch (ev->code) {
        case KEY_ESC: return event_result_response((WidgetResponse){ .result = NULL, .cancelled = true, .error = NULL });
        case KEY_TAB: d->focused = (d->focused + 1) % (d->field_count + 1); d->dirty = true; return event_result_handled();
        case KEY_BACKTAB: d->focused = d->focused > 0 ? d->focused - 1 : d->field_count; d->dirty = true; return event_result_handled();
        case KEY_ENTER:
            if (d->focused == d->field_count) { cJSON *obj = cJSON_CreateObject(); for (int i = 0; i < d->field_count; i++) cJSON_AddStringToObject(obj, d->fields[i].label, d->fields[i].value); return event_result_response((WidgetResponse){ .result = obj, .cancelled = false, .error = NULL }); }
            return event_result_handled();
        case KEY_CHAR: if (d->focused < d->field_count) { int len = strlen(d->fields[d->focused].value); d->fields[d->focused].value = realloc(d->fields[d->focused].value, len + 2); d->fields[d->focused].value[len] = ev->ch; d->fields[d->focused].value[len + 1] = '\0'; d->dirty = true; } return event_result_handled();
        case KEY_BACKSPACE: if (d->focused < d->field_count && strlen(d->fields[d->focused].value) > 0) { d->fields[d->focused].value[strlen(d->fields[d->focused].value) - 1] = '\0'; d->dirty = true; } return event_result_handled();
        default: return event_result_unhandled();
    }
}

static bool form_is_dirty(Widget *self) { return ((FormData *)(self + 1))->dirty; }
static void form_clear_dirty(Widget *self) { ((FormData *)(self + 1))->dirty = false; }
static void form_destroy(Widget *self) {
    FormData *d = (FormData *)(self + 1); free(d->title); free(d->submit_label);
    for (int i = 0; i < d->field_count; i++) { free(d->fields[i].label); free(d->fields[i].widget_type); free(d->fields[i].value); free(d->fields[i].placeholder); for (int j = 0; j < d->fields[i].choice_count; j++) free(d->fields[i].choices[j]); free(d->fields[i].choices); }
    free(d->fields);
}

Widget *form_widget_new(const char *title, FormField *fields, int field_count, const char *submit_label) {
    Widget *w = calloc(1, sizeof(Widget) + sizeof(FormData));
    w->vtable.render = form_render; w->vtable.handle_event = form_handle_event;
    w->vtable.is_dirty = form_is_dirty; w->vtable.clear_dirty = form_clear_dirty; w->vtable.destroy = form_destroy;
    FormData *d = (FormData *)(w + 1); d->title = strdup(title); d->fields = malloc(field_count * sizeof(FormField)); d->field_count = field_count;
    for (int i = 0; i < field_count; i++) { d->fields[i].label = strdup(fields[i].label); d->fields[i].widget_type = strdup(fields[i].widget_type); d->fields[i].value = strdup(fields[i].value); d->fields[i].choices = fields[i].choices ? malloc(fields[i].choice_count * sizeof(char *)) : NULL; d->fields[i].choice_count = fields[i].choice_count; for (int j = 0; j < fields[i].choice_count; j++) d->fields[i].choices[j] = strdup(fields[i].choices[j]); d->fields[i].placeholder = strdup(fields[i].placeholder); }
    d->focused = 0; d->submit_label = strdup(submit_label); d->dirty = true;
    return w;
}