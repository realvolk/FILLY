#include "form.h"
#include "core/widget_base.h"
#include "core/session.h"
#include <stdlib.h>
#include <string.h>

typedef struct { WidgetBase base; char *title, *submit_label; FormField *fields; int field_count, focused; } FormData;
extern Arena *g_session_arena;

static void form_render(Widget *self, Rect area, RenderTree *out) {
    FormData *d = (FormData *)(self + 1);
    memset(out, 0, sizeof(*out)); out->style_class = "container";
    int box_w = 60, box_h = 4 + d->field_count * 2 + 2; if (box_w > area.w - 2) box_w = area.w - 2; if (box_h > area.h - 2) box_h = area.h - 2;
    RenderTree *children = arena_alloc(g_session_arena, 3 * sizeof(RenderTree));
    children[0].type = RNODE_TEXT; children[0].rect = rect_new(1, 0, box_w - 2, 1);
    children[0].text.content = arena_strdup(g_session_arena, d->title); children[0].style_class = "text"; children[0].state = "title";
    children[1].type = RNODE_FORM; children[1].rect = rect_new(1, 1, box_w - 2, box_h - 3);
    children[1].form.fields = d->fields; children[1].form.field_count = d->field_count; children[1].form.focused = d->focused;
    children[1].form.submit_label = arena_strdup(g_session_arena, d->submit_label); children[1].style_class = "form";
    children[2].type = RNODE_TEXT; children[2].rect = rect_new(1, box_h - 2, box_w - 2, 1);
    children[2].text.content = "Tab:next  Enter:submit  Esc:cancel"; children[2].style_class = "text"; children[2].state = "muted";
    out->type = RNODE_CONTAINER; out->rect = rect_new((area.w - box_w) / 2, (area.h - box_h) / 2, box_w, box_h);
    out->container.border = BORDER_SINGLE; out->container.padding = edgeinsets_zero();
    out->container.children = children; out->container.child_count = 3;
}

static EventResult form_handle_event(Widget *self, Event *ev, Backend *backend) {
    (void)backend; FormData *d = (FormData *)(self + 1);
    if (ev->type != EVENT_KEY) return event_result_unhandled();
    switch (ev->code) {
        case KEY_ESC: return event_result_response((WidgetResponse){ .result = NULL, .cancelled = true });
        case KEY_TAB:
            d->focused = (d->focused + 1) % (d->field_count + 1);
            d->base.accepts_text_input = (d->focused < d->field_count);
            d->base.dirty = true;
            return event_result_handled();
        case KEY_BACKTAB:
            d->focused = d->focused > 0 ? d->focused - 1 : d->field_count;
            d->base.accepts_text_input = (d->focused < d->field_count);
            d->base.dirty = true;
            return event_result_handled();
        case KEY_ENTER:
            if (d->focused == d->field_count) {
                cJSON *obj = cJSON_CreateObject();
                for (int i=0;i<d->field_count;i++) cJSON_AddStringToObject(obj, d->fields[i].label, d->fields[i].value);
                return event_result_response((WidgetResponse){ .result = obj, .cancelled = false });
            }
            return event_result_handled();
        case KEY_CHAR:
            if (d->focused < d->field_count) {
                int len = strlen(d->fields[d->focused].value);
                d->fields[d->focused].value = realloc(d->fields[d->focused].value, len+2);
                d->fields[d->focused].value[len] = ev->ch;
                d->fields[d->focused].value[len+1] = '\0';
                d->base.dirty = true;
            }
            return event_result_handled();
        case KEY_BACKSPACE:
            if (d->focused < d->field_count && strlen(d->fields[d->focused].value) > 0) {
                d->fields[d->focused].value[strlen(d->fields[d->focused].value)-1] = '\0';
                d->base.dirty = true;
            }
            return event_result_handled();
        default: return event_result_unhandled();
    }
}

static void form_destroy(Widget *self) { FormData *d = (FormData *)(self + 1); free(d->title); free(d->submit_label); for (int i=0;i<d->field_count;i++){free(d->fields[i].label);free(d->fields[i].widget_type);free(d->fields[i].value);free(d->fields[i].placeholder);for(int j=0;j<d->fields[i].choice_count;j++)free(d->fields[i].choices[j]);free(d->fields[i].choices);} free(d->fields); }

Widget *form_widget_new(const char *title, FormField *fields, int field_count, const char *submit_label) {
    Widget *w = calloc(1, sizeof(Widget) + sizeof(FormData));
    FormData data = { .title = strdup(title), .field_count = field_count, .focused = 0, .submit_label = strdup(submit_label) };
    data.fields = malloc(field_count * sizeof(FormField));
    for (int i=0;i<field_count;i++){ data.fields[i].label=strdup(fields[i].label); data.fields[i].widget_type=strdup(fields[i].widget_type); data.fields[i].value=strdup(fields[i].value); data.fields[i].placeholder=strdup(fields[i].placeholder); data.fields[i].choices=fields[i].choices?malloc(fields[i].choice_count*sizeof(char*)):NULL; data.fields[i].choice_count=fields[i].choice_count; for(int j=0;j<fields[i].choice_count;j++) data.fields[i].choices[j]=strdup(fields[i].choices[j]); }
    widget_base_init(w, &data, sizeof(FormData), form_render, form_handle_event, form_destroy);
    WidgetBase *base = (WidgetBase *)(w + 1);
    base->accepts_text_input = true;
    return w;
}