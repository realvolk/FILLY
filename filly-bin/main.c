#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "../filly-core/widgets/menu.h"
#include "../filly-core/widgets/yesno.h"
#include "../filly-core/widgets/input.h"
#include "../filly-core/widgets/password.h"
#include "../filly-core/widgets/checklist.h"
#include "../filly-core/widgets/msg.h"
#include "../filly-core/widgets/filter.h"
#include "../filly-core/widgets/multiselect.h"
#include "../filly-core/widgets/file_picker.h"
#include "../filly-core/widgets/text_editor.h"
#include "../filly-core/widgets/summary.h"
#include "../filly-core/widgets/progress.h"
#include "../filly-core/widgets/toggle.h"
#include "../filly-core/widgets/spinner.h"
#include "../filly-core/widgets/separator.h"
#include "../filly-core/widgets/disk.h"
#include "../filly-core/widgets/table.h"
#include "../filly-core/widgets/gauge.h"
#include "../filly-core/widgets/form.h"
#include "../filly-core/widgets/calendar.h"
#include "../filly-core/widgets/context_menu.h"
#include "../filly-core/widgets/notification.h"
#include "../filly-core/widgets/radio_group.h"
#include "../filly-core/widgets/range_slider.h"
#include "../filly-core/widgets/color_picker.h"
#include "../filly-core/widgets/badge.h"
#include "../filly-core/widgets/rich_text.h"
#include "../filly-core/widgets/tooltip.h"
#include "../filly-core/widgets/hub.h"
#include "../filly-core/widgets/tabs.h"
#include "../filly-core/widgets/split_panes.h"
#include "../filly-core/widgets/tree.h"
#include <getopt.h>
#include "../filly-protocol/protocol.h"
#include "../filly-terminal/terminal.h"
#include "../filly-core/widget.h"
#include "../filly-core/session.h"
#include "../filly-core/theme.h"
#include "../filly-core/store.h"
#include "../filly-daemon/daemon.h"

extern Widget *menu_widget_factory(const WidgetRequest *req);
extern Widget *yesno_widget_factory(const WidgetRequest *req);
extern Widget *input_widget_factory(const WidgetRequest *req);
extern Widget *password_widget_factory(const WidgetRequest *req);
extern Widget *checklist_widget_factory(const WidgetRequest *req);
extern Widget *msg_widget_factory(const WidgetRequest *req);
extern Widget *filter_widget_factory(const WidgetRequest *req);
extern Widget *multiselect_widget_factory(const WidgetRequest *req);
extern Widget *file_picker_widget_factory(const WidgetRequest *req);
extern Widget *text_editor_widget_factory(const WidgetRequest *req);
extern Widget *summary_widget_factory(const WidgetRequest *req);
extern Widget *progress_widget_factory(const WidgetRequest *req);
extern Widget *toggle_widget_factory(const WidgetRequest *req);
extern Widget *spinner_widget_factory(const WidgetRequest *req);
extern Widget *separator_widget_factory(const WidgetRequest *req);
extern Widget *disk_widget_factory(const WidgetRequest *req);
extern Widget *table_widget_factory(const WidgetRequest *req);
extern Widget *tree_widget_factory(const WidgetRequest *req);
extern Widget *gauge_widget_factory(const WidgetRequest *req);
extern Widget *calendar_widget_factory(const WidgetRequest *req);
extern Widget *form_widget_factory(const WidgetRequest *req);
extern Widget *tabs_widget_factory(const WidgetRequest *req);
extern Widget *split_panes_widget_factory(const WidgetRequest *req);
extern Widget *context_menu_widget_factory(const WidgetRequest *req);
extern Widget *notification_widget_factory(const WidgetRequest *req);
extern Widget *radio_group_widget_factory(const WidgetRequest *req);
extern Widget *range_slider_widget_factory(const WidgetRequest *req);
extern Widget *color_picker_widget_factory(const WidgetRequest *req);
extern Widget *badge_widget_factory(const WidgetRequest *req);
extern Widget *rich_text_widget_factory(const WidgetRequest *req);
extern Widget *tooltip_widget_factory(const WidgetRequest *req);
extern Widget *hub_widget_factory(const WidgetRequest *req);
extern BackendVTable terminal_vtable;
extern int relay_main(const char *sock_path);

typedef struct {
    char *title;
    char *text;
    bool dirty;
} PaneData;

static void pane_render(Widget *self, Rect area, RenderTree *out) {
    PaneData *d = (PaneData *)(self + 1);
    memset(out, 0, sizeof(*out));
    out->type = RNODE_CONTAINER;
    out->rect = rect_new(0, 0, area.w, area.h);
    out->container.border = BORDER_SINGLE;
    out->container.padding = edgeinsets_zero();
    RenderTree *children = calloc(2, sizeof(RenderTree));
    children[0].type = RNODE_TEXT;
    children[0].rect = rect_new(1, 0, area.w - 2, 1);
    children[0].text.content = strdup(d->title);
    children[0].text.align = ALIGN_CENTER;
    children[0].text.style = textstyle_selected();
    children[1].type = RNODE_TEXT;
    children[1].rect = rect_new(1, 1, area.w - 2, area.h - 2);
    children[1].text.content = strdup(d->text);
    children[1].text.align = ALIGN_LEFT;
    children[1].text.style = textstyle_normal();
    out->container.children = children;
    out->container.child_count = 2;
}

static EventResult pane_handle(Widget *self, Event *ev, Backend *backend) {
    (void)self; (void)ev; (void)backend;
    return event_result_unhandled();
}

static bool pane_is_dirty(Widget *self) { return ((PaneData *)(self + 1))->dirty; }
static void pane_clear_dirty(Widget *self) { ((PaneData *)(self + 1))->dirty = false; }
static void pane_destroy(Widget *self) {
    PaneData *d = (PaneData *)(self + 1);
    free(d->title); free(d->text);
}

static Widget *pane_widget_new(const char *title, const char *text) {
    Widget *w = calloc(1, sizeof(Widget) + sizeof(PaneData));
    w->vtable.render = pane_render;
    w->vtable.handle_event = pane_handle;
    w->vtable.is_dirty = pane_is_dirty;
    w->vtable.clear_dirty = pane_clear_dirty;
    w->vtable.destroy = pane_destroy;
    PaneData *d = (PaneData *)(w + 1);
    d->title = strdup(title);
    d->text = strdup(text);
    d->dirty = true;
    return w;
}

static void register_builtin_widgets(void) {
    widget_registry_register("menu", menu_widget_factory);
    widget_registry_register("yesno", yesno_widget_factory);
    widget_registry_register("input", input_widget_factory);
    widget_registry_register("password", password_widget_factory);
    widget_registry_register("checklist", checklist_widget_factory);
    widget_registry_register("msg", msg_widget_factory);
    widget_registry_register("filter", filter_widget_factory);
    widget_registry_register("multiselect", multiselect_widget_factory);
    widget_registry_register("file_picker", file_picker_widget_factory);
    widget_registry_register("text", text_editor_widget_factory);
    widget_registry_register("summary", summary_widget_factory);
    widget_registry_register("progress", progress_widget_factory);
    widget_registry_register("toggle", toggle_widget_factory);
    widget_registry_register("spinner", spinner_widget_factory);
    widget_registry_register("separator", separator_widget_factory);
    widget_registry_register("disk", disk_widget_factory);
    widget_registry_register("table", table_widget_factory);
    widget_registry_register("tree", tree_widget_factory);
    widget_registry_register("gauge", gauge_widget_factory);
    widget_registry_register("calendar", calendar_widget_factory);
    widget_registry_register("form", form_widget_factory);
    widget_registry_register("tabs", tabs_widget_factory);
    widget_registry_register("split_panes", split_panes_widget_factory);
    widget_registry_register("context_menu", context_menu_widget_factory);
    widget_registry_register("notification", notification_widget_factory);
    widget_registry_register("radio_group", radio_group_widget_factory);
    widget_registry_register("range_slider", range_slider_widget_factory);
    widget_registry_register("color_picker", color_picker_widget_factory);
    widget_registry_register("badge", badge_widget_factory);
    widget_registry_register("rich_text", rich_text_widget_factory);
    widget_registry_register("tooltip", tooltip_widget_factory);
    widget_registry_register("hub", hub_widget_factory);
}

Widget *menu_widget_factory(const WidgetRequest *req) {
    cJSON *t = cJSON_GetObjectItem(req->params, "title");
    cJSON *m = cJSON_GetObjectItem(req->params, "message");
    cJSON *ch = cJSON_GetObjectItem(req->params, "choices");
    int count = ch ? cJSON_GetArraySize(ch) : 0;
    char **choices = malloc(count * sizeof(char *));
    for (int i = 0; i < count; i++) choices[i] = strdup(cJSON_GetArrayItem(ch, i)->valuestring);
    cJSON *def = cJSON_GetObjectItem(req->params, "default");
    int def_idx = 0;
    if (def && def->valuestring)
        for (int i = 0; i < count; i++)
            if (strcmp(choices[i], def->valuestring) == 0) { def_idx = i; break; }
    Widget *w = menu_widget_new(t ? t->valuestring : "", m ? m->valuestring : "", choices, count, def_idx);
    for (int i = 0; i < count; i++) free(choices[i]);
    free(choices);
    return w;
}

Widget *yesno_widget_factory(const WidgetRequest *req) {
    cJSON *t = cJSON_GetObjectItem(req->params, "title");
    cJSON *m = cJSON_GetObjectItem(req->params, "message");
    cJSON *d = cJSON_GetObjectItem(req->params, "default");
    return yesno_widget_new(t ? t->valuestring : "", m ? m->valuestring : "", d ? d->valueint : true);
}

Widget *input_widget_factory(const WidgetRequest *req) {
    cJSON *t = cJSON_GetObjectItem(req->params, "title");
    cJSON *m = cJSON_GetObjectItem(req->params, "message");
    cJSON *d = cJSON_GetObjectItem(req->params, "default");
    cJSON *p = cJSON_GetObjectItem(req->params, "placeholder");
    cJSON *v = cJSON_GetObjectItem(req->params, "validation");
    return input_widget_new(t ? t->valuestring : "", m ? m->valuestring : "",
        d ? d->valuestring : "", p ? p->valuestring : "", v ? v->valuestring : NULL);
}

Widget *password_widget_factory(const WidgetRequest *req) {
    cJSON *t = cJSON_GetObjectItem(req->params, "title");
    cJSON *m = cJSON_GetObjectItem(req->params, "message");
    cJSON *p = cJSON_GetObjectItem(req->params, "placeholder");
    return password_widget_new(t ? t->valuestring : "", m ? m->valuestring : "", p ? p->valuestring : "");
}

Widget *checklist_widget_factory(const WidgetRequest *req) {
    cJSON *t = cJSON_GetObjectItem(req->params, "title");
    cJSON *m = cJSON_GetObjectItem(req->params, "message");
    cJSON *ch = cJSON_GetObjectItem(req->params, "choices");
    int count = ch ? cJSON_GetArraySize(ch) : 0;
    char **choices = malloc(count * sizeof(char *));
    for (int i = 0; i < count; i++) choices[i] = strdup(cJSON_GetArrayItem(ch, i)->valuestring);
    cJSON *min = cJSON_GetObjectItem(req->params, "min");
    cJSON *max = cJSON_GetObjectItem(req->params, "max");
    cJSON *def = cJSON_GetObjectItem(req->params, "default");
    int dcount = def ? cJSON_GetArraySize(def) : 0;
    char **defaults = malloc(dcount * sizeof(char *));
    for (int i = 0; i < dcount; i++) defaults[i] = strdup(cJSON_GetArrayItem(def, i)->valuestring);
    Widget *w = checklist_widget_new(t ? t->valuestring : "", m ? m->valuestring : "", choices, count,
        min ? min->valueint : 0, max ? max->valueint : count, defaults, dcount);
    for (int i = 0; i < count; i++) free(choices[i]);
    free(choices);
    for (int i = 0; i < dcount; i++) free(defaults[i]);
    free(defaults);
    return w;
}

Widget *msg_widget_factory(const WidgetRequest *req) {
    cJSON *t = cJSON_GetObjectItem(req->params, "title");
    cJSON *m = cJSON_GetObjectItem(req->params, "message");
    return msg_widget_new(t ? t->valuestring : "", m ? m->valuestring : "");
}

Widget *filter_widget_factory(const WidgetRequest *req) {
    cJSON *t = cJSON_GetObjectItem(req->params, "title");
    cJSON *m = cJSON_GetObjectItem(req->params, "message");
    cJSON *ch = cJSON_GetObjectItem(req->params, "choices");
    int count = ch ? cJSON_GetArraySize(ch) : 0;
    char **choices = malloc(count * sizeof(char *));
    for (int i = 0; i < count; i++) choices[i] = strdup(cJSON_GetArrayItem(ch, i)->valuestring);
    cJSON *p = cJSON_GetObjectItem(req->params, "placeholder");
    Widget *w = filter_widget_new(t ? t->valuestring : "", m ? m->valuestring : "", choices, count, p ? p->valuestring : NULL);
    for (int i = 0; i < count; i++) free(choices[i]);
    free(choices);
    return w;
}

Widget *multiselect_widget_factory(const WidgetRequest *req) {
    cJSON *t = cJSON_GetObjectItem(req->params, "title");
    cJSON *m = cJSON_GetObjectItem(req->params, "message");
    cJSON *ch = cJSON_GetObjectItem(req->params, "choices");
    int count = ch ? cJSON_GetArraySize(ch) : 0;
    char **choices = malloc(count * sizeof(char *));
    for (int i = 0; i < count; i++) choices[i] = strdup(cJSON_GetArrayItem(ch, i)->valuestring);
    cJSON *p = cJSON_GetObjectItem(req->params, "placeholder");
    cJSON *min = cJSON_GetObjectItem(req->params, "min");
    cJSON *max = cJSON_GetObjectItem(req->params, "max");
    Widget *w = multiselect_widget_new(t ? t->valuestring : "", m ? m->valuestring : "", choices, count,
        p ? p->valuestring : NULL, min ? min->valueint : 0, max ? max->valueint : count);
    for (int i = 0; i < count; i++) free(choices[i]);
    free(choices);
    return w;
}

Widget *file_picker_widget_factory(const WidgetRequest *req) {
    cJSON *t = cJSON_GetObjectItem(req->params, "title");
    cJSON *s = cJSON_GetObjectItem(req->params, "start_dir");
    cJSON *f = cJSON_GetObjectItem(req->params, "filter");
    return file_picker_widget_new(t ? t->valuestring : "", s ? s->valuestring : NULL, f ? f->valuestring : NULL);
}

Widget *text_editor_widget_factory(const WidgetRequest *req) {
    cJSON *t = cJSON_GetObjectItem(req->params, "title");
    cJSON *f = cJSON_GetObjectItem(req->params, "file");
    cJSON *c = cJSON_GetObjectItem(req->params, "content");
    return text_editor_widget_new(t ? t->valuestring : "", f ? f->valuestring : NULL, c ? c->valuestring : NULL);
}

Widget *summary_widget_factory(const WidgetRequest *req) {
    cJSON *t = cJSON_GetObjectItem(req->params, "title");
    cJSON *m = cJSON_GetObjectItem(req->params, "message");
    cJSON *f = cJSON_GetObjectItem(req->params, "file");
    return summary_widget_new(t ? t->valuestring : "", m ? m->valuestring : NULL, f ? f->valuestring : NULL);
}

Widget *progress_widget_factory(const WidgetRequest *req) {
    cJSON *t = cJSON_GetObjectItem(req->params, "title");
    cJSON *cmd = cJSON_GetObjectItem(req->params, "command");
    int count = cmd ? cJSON_GetArraySize(cmd) : 0;
    char **command = malloc(count * sizeof(char *));
    for (int i = 0; i < count; i++) command[i] = strdup(cJSON_GetArrayItem(cmd, i)->valuestring);
    cJSON *lf = cJSON_GetObjectItem(req->params, "logfile");
    Widget *w = progress_widget_new(t ? t->valuestring : "", command, count, lf ? lf->valuestring : NULL);
    for (int i = 0; i < count; i++) free(command[i]);
    free(command);
    return w;
}

Widget *toggle_widget_factory(const WidgetRequest *req) {
    cJSON *t = cJSON_GetObjectItem(req->params, "title");
    cJSON *l = cJSON_GetObjectItem(req->params, "label");
    cJSON *d = cJSON_GetObjectItem(req->params, "default");
    return toggle_widget_new(t ? t->valuestring : "", l ? l->valuestring : "", d ? d->valueint : false);
}

Widget *spinner_widget_factory(const WidgetRequest *req) {
    cJSON *m = cJSON_GetObjectItem(req->params, "message");
    return spinner_widget_new(m ? m->valuestring : "");
}

Widget *separator_widget_factory(const WidgetRequest *req) {
    cJSON *o = cJSON_GetObjectItem(req->params, "orientation");
    return separator_widget_new(o && strcmp(o->valuestring, "vertical") == 0 ? ORIENT_VERTICAL : ORIENT_HORIZONTAL);
}

Widget *disk_widget_factory(const WidgetRequest *req) {
    cJSON *t = cJSON_GetObjectItem(req->params, "title");
    cJSON *d = cJSON_GetObjectItem(req->params, "disk");
    cJSON *p = cJSON_GetObjectItem(req->params, "partitions");
    cJSON *fs = cJSON_GetObjectItem(req->params, "free_space");
    cJSON *ro = cJSON_GetObjectItem(req->params, "readonly");
    return disk_widget_new(t ? t->valuestring : "", d ? d->valuestring : "", p, fs, ro ? ro->valueint : false);
}

Widget *table_widget_factory(const WidgetRequest *req) {
    cJSON *t = cJSON_GetObjectItem(req->params, "title");
    cJSON *h = cJSON_GetObjectItem(req->params, "headers");
    cJSON *r = cJSON_GetObjectItem(req->params, "rows");
    int hc = h ? cJSON_GetArraySize(h) : 0;
    int rc = r ? cJSON_GetArraySize(r) : 0;
    char **headers = malloc(hc * sizeof(char *));
    for (int i = 0; i < hc; i++) headers[i] = strdup(cJSON_GetArrayItem(h, i)->valuestring);
    char ***rows = malloc(rc * sizeof(char **));
    for (int i = 0; i < rc; i++) {
        cJSON *row = cJSON_GetArrayItem(r, i);
        rows[i] = malloc(hc * sizeof(char *));
        for (int j = 0; j < hc; j++) rows[i][j] = strdup(cJSON_GetArrayItem(row, j)->valuestring);
    }
    Widget *w = table_widget_new(t ? t->valuestring : "", headers, hc, rows, rc);
    for (int i = 0; i < hc; i++) free(headers[i]);
    free(headers);
    for (int i = 0; i < rc; i++) { for (int j = 0; j < hc; j++) free(rows[i][j]); free(rows[i]); }
    free(rows);
    return w;
}

Widget *gauge_widget_factory(const WidgetRequest *req) {
    cJSON *t = cJSON_GetObjectItem(req->params, "title");
    cJSON *p = cJSON_GetObjectItem(req->params, "percent");
    cJSON *l = cJSON_GetObjectItem(req->params, "label");
    return gauge_widget_new(t ? t->valuestring : "", p ? p->valueint : 0, l ? l->valuestring : "");
}

Widget *calendar_widget_factory(const WidgetRequest *req) {
    cJSON *t = cJSON_GetObjectItem(req->params, "title");
    return calendar_widget_new(t ? t->valuestring : "");
}

Widget *context_menu_widget_factory(const WidgetRequest *req) {
    cJSON *items = cJSON_GetObjectItem(req->params, "items");
    int count = items ? cJSON_GetArraySize(items) : 0;
    char **arr = malloc(count * sizeof(char *));
    for (int i = 0; i < count; i++) arr[i] = strdup(cJSON_GetArrayItem(items, i)->valuestring);
    Widget *w = context_menu_widget_new(arr, count);
    for (int i = 0; i < count; i++) free(arr[i]);
    free(arr);
    return w;
}

Widget *notification_widget_factory(const WidgetRequest *req) {
    cJSON *m = cJSON_GetObjectItem(req->params, "message");
    cJSON *d = cJSON_GetObjectItem(req->params, "duration");
    return notification_widget_new(m ? m->valuestring : "", d ? d->valueint : 3);
}

Widget *radio_group_widget_factory(const WidgetRequest *req) {
    cJSON *t = cJSON_GetObjectItem(req->params, "title");
    cJSON *m = cJSON_GetObjectItem(req->params, "message");
    cJSON *ch = cJSON_GetObjectItem(req->params, "choices");
    int count = ch ? cJSON_GetArraySize(ch) : 0;
    char **choices = malloc(count * sizeof(char *));
    for (int i = 0; i < count; i++) choices[i] = strdup(cJSON_GetArrayItem(ch, i)->valuestring);
    cJSON *def = cJSON_GetObjectItem(req->params, "default");
    Widget *w = radio_group_widget_new(t ? t->valuestring : "", m ? m->valuestring : "", choices, count, def ? def->valueint : 0);
    for (int i = 0; i < count; i++) free(choices[i]);
    free(choices);
    return w;
}

Widget *range_slider_widget_factory(const WidgetRequest *req) {
    cJSON *t = cJSON_GetObjectItem(req->params, "title");
    cJSON *min = cJSON_GetObjectItem(req->params, "min");
    cJSON *max = cJSON_GetObjectItem(req->params, "max");
    cJSON *val = cJSON_GetObjectItem(req->params, "value");
    cJSON *l = cJSON_GetObjectItem(req->params, "label");
    return range_slider_widget_new(t ? t->valuestring : "", min ? min->valueint : 0, max ? max->valueint : 100, val ? val->valueint : 50, l ? l->valuestring : "");
}

Widget *color_picker_widget_factory(const WidgetRequest *req) {
    cJSON *t = cJSON_GetObjectItem(req->params, "title");
    cJSON *c = cJSON_GetObjectItem(req->params, "colors");
    int count = c ? cJSON_GetArraySize(c) : 0;
    char **colors = malloc(count * sizeof(char *));
    for (int i = 0; i < count; i++) colors[i] = strdup(cJSON_GetArrayItem(c, i)->valuestring);
    Widget *w = color_picker_widget_new(t ? t->valuestring : "", colors, count);
    for (int i = 0; i < count; i++) free(colors[i]);
    free(colors);
    return w;
}

Widget *badge_widget_factory(const WidgetRequest *req) {
    cJSON *text = cJSON_GetObjectItem(req->params, "text");
    return badge_widget_new(text ? text->valuestring : "");
}

Widget *rich_text_widget_factory(const WidgetRequest *req) {
    cJSON *c = cJSON_GetObjectItem(req->params, "content");
    return rich_text_widget_new(c ? c->valuestring : "");
}

Widget *tooltip_widget_factory(const WidgetRequest *req) {
    cJSON *text = cJSON_GetObjectItem(req->params, "text");
    return tooltip_widget_new(text ? text->valuestring : "");
}

Widget *hub_widget_factory(const WidgetRequest *req) {
    cJSON *t = cJSON_GetObjectItem(req->params, "title");
    cJSON *cat = cJSON_GetObjectItem(req->params, "categories");
    cJSON *act = cJSON_GetObjectItem(req->params, "actions");
    return hub_widget_new(t ? t->valuestring : "", cat, act);
}

Widget *form_widget_factory(const WidgetRequest *req) {
    cJSON *t = cJSON_GetObjectItem(req->params, "title");
    cJSON *fields_json = cJSON_GetObjectItem(req->params, "fields");
    cJSON *submit = cJSON_GetObjectItem(req->params, "submit_label");
    int count = fields_json ? cJSON_GetArraySize(fields_json) : 0;
    FormField *fields = calloc(count, sizeof(FormField));
    for (int i = 0; i < count; i++) {
        cJSON *f = cJSON_GetArrayItem(fields_json, i);
        cJSON *label_j = cJSON_GetObjectItem(f, "label");
        cJSON *wtype_j = cJSON_GetObjectItem(f, "widget_type");
        cJSON *value_j = cJSON_GetObjectItem(f, "value");
        cJSON *ph_j = cJSON_GetObjectItem(f, "placeholder");
        fields[i].label = strdup(label_j && label_j->valuestring ? label_j->valuestring : "");
        fields[i].widget_type = strdup(wtype_j && wtype_j->valuestring ? wtype_j->valuestring : "input");
        fields[i].value = strdup(value_j && value_j->valuestring ? value_j->valuestring : "");
        fields[i].placeholder = strdup(ph_j && ph_j->valuestring ? ph_j->valuestring : "");
        cJSON *ch = cJSON_GetObjectItem(f, "choices");
        fields[i].choice_count = ch ? cJSON_GetArraySize(ch) : 0;
        fields[i].choices = fields[i].choice_count > 0 ? malloc(fields[i].choice_count * sizeof(char *)) : NULL;
        for (int j = 0; j < fields[i].choice_count; j++)
            fields[i].choices[j] = strdup(cJSON_GetArrayItem(ch, j)->valuestring);
    }
    Widget *w = form_widget_new(t ? t->valuestring : "", fields, count, submit ? submit->valuestring : "Submit");
    for (int i = 0; i < count; i++) {
        free(fields[i].label); free(fields[i].widget_type); free(fields[i].value); free(fields[i].placeholder);
        for (int j = 0; j < fields[i].choice_count; j++) free(fields[i].choices[j]);
        free(fields[i].choices);
    }
    free(fields);
    return w;
}

Widget *tabs_widget_factory(const WidgetRequest *req) {
    cJSON *t = cJSON_GetObjectItem(req->params, "title");
    cJSON *tabs_json = cJSON_GetObjectItem(req->params, "tabs");
    int count = tabs_json ? cJSON_GetArraySize(tabs_json) : 0;
    char **labels = malloc(count * sizeof(char *));
    Widget **children = malloc(count * sizeof(Widget *));
    for (int i = 0; i < count; i++) {
        cJSON *tab = cJSON_GetArrayItem(tabs_json, i);
        cJSON *label_j = cJSON_GetObjectItem(tab, "label");
        labels[i] = strdup(label_j && label_j->valuestring ? label_j->valuestring : "");
        children[i] = msg_widget_new(labels[i], "");
    }
    Widget *w = tabs_widget_new(t ? t->valuestring : "", labels, count, children, count);
    for (int i = 0; i < count; i++) free(labels[i]);
    free(labels); free(children);
    return w;
}

Widget *split_panes_widget_factory(const WidgetRequest *req) {
    cJSON *o = cJSON_GetObjectItem(req->params, "orientation");
    Orientation orient = (o && strcmp(o->valuestring, "vertical") == 0) ? ORIENT_VERTICAL : ORIENT_HORIZONTAL;
    Widget *first = pane_widget_new("Pane 1", "Left / Top content");
    Widget *second = pane_widget_new("Pane 2", "Right / Bottom content");
    return split_panes_widget_new(orient, first, second);
}

Widget *tree_widget_factory(const WidgetRequest *req) {
    cJSON *t = cJSON_GetObjectItem(req->params, "title");
    TreeNode nodes[3] = {
        { .label = "src", .expanded = true, .children = NULL, .child_count = 0 },
        { .label = "include", .expanded = false, .children = NULL, .child_count = 0 },
        { .label = "Makefile", .expanded = false, .children = NULL, .child_count = 0 },
    };
    return tree_widget_new(t ? t->valuestring : "Tree", nodes, 3);
}

static void print_usage(void) {
    fprintf(stderr, "Usage: filly [command]\n");
    fprintf(stderr, "  daemon [--socket path] [--theme name]\n");
    fprintf(stderr, "  oneshot [--input file] [--output file] [--theme name]\n");
    fprintf(stderr, "  batch [--input file]\n");
    fprintf(stderr, "  demo [--theme name]\n");
    fprintf(stderr, "  validate --input file\n");
}

int main(int argc, char **argv) {
    if (argc < 2) {
        register_builtin_widgets();
        TerminalBackend t;
        terminal_backend_init(&t, NULL);
        Backend backend = { .vtable = &terminal_vtable, .data = &t };
        Widget *w = msg_widget_new("FILLY", "No command given. Try 'filly demo'.");
        session_run(w, &backend);
        widget_destroy(w);
        terminal_backend_destroy(&t);
        return 0;
    }

    register_builtin_widgets();
    load_plugins();

    if (strcmp(argv[1], "daemon") == 0) {
        const char *socket = "/tmp/filly.sock";
        for (int i = 2; i < argc; i++) {
            if (strcmp(argv[i], "--socket") == 0 && i + 1 < argc) socket = argv[++i];
        }
        return daemon_run(socket) ? 0 : 1;
    }

    if (strcmp(argv[1], "oneshot") == 0) {
        const char *input = NULL;
        for (int i = 2; i < argc; i++) {
            if (strcmp(argv[i], "--input") == 0 && i + 1 < argc) input = argv[++i];
        }
        char *json = NULL;
        if (input) {
            FILE *f = fopen(input, "r");
            if (f) {
                fseek(f, 0, SEEK_END);
                long sz = ftell(f);
                rewind(f);
                json = malloc(sz + 1);
                fread(json, 1, sz, f);
                json[sz] = '\0';
                fclose(f);
            }
        } else {
            char buf[65536];
            int n = read(STDIN_FILENO, buf, sizeof(buf) - 1);
            if (n > 0) { buf[n] = '\0'; json = strdup(buf); }
        }
        if (!json || strlen(json) == 0) { fprintf(stderr, "Empty input\n"); return 1; }
        WidgetRequest *req = widget_request_parse(json);
        if (!req) { fprintf(stderr, "Invalid JSON\n"); free(json); return 1; }
        free(json);
        Widget *w = widget_registry_create(req);
        if (!w) { fprintf(stderr, "Unknown widget: %s\n", req->widget); widget_request_free(req); return 1; }
        TerminalBackend t;
        terminal_backend_init(&t, NULL);
        Backend backend = { .vtable = &terminal_vtable, .data = &t };
        WidgetResponse resp = session_run(w, &backend);
        char *out = widget_response_to_json(&resp);
        printf("\n%s\n", out);
        free(out);
        widget_destroy(w);
        widget_request_free(req);
        terminal_backend_destroy(&t);
        return resp.cancelled ? 1 : 0;
    }

    if (strcmp(argv[1], "demo") == 0) {
        TerminalBackend t;
        terminal_backend_init(&t, NULL);
        Backend backend = { .vtable = &terminal_vtable, .data = &t };
        Widget *w;

        w = msg_widget_new("FILLY Demo", "Welcome! Press any key to continue.");
        session_run(w, &backend); widget_destroy(w);

        w = yesno_widget_new("Yes/No", "Is FILLY-C working?", true);
        session_run(w, &backend); widget_destroy(w);

        char *menu_choices[] = {"KDE", "XFCE", "Sway", "Hyprland", "i3"};
        w = menu_widget_new("Menu", "Choose desktop:", menu_choices, 5, 0);
        session_run(w, &backend); widget_destroy(w);

        w = input_widget_new("Input", "Username:", "artix", "username", NULL);
        session_run(w, &backend); widget_destroy(w);

        w = password_widget_new("Password", "Secret:", "password");
        session_run(w, &backend); widget_destroy(w);

        char *check_choices[] = {"git", "neovim", "firefox", "alacritty", "tmux"};
        char *check_defaults[] = {"git", "neovim"};
        w = checklist_widget_new("Checklist", "Select packages:", check_choices, 5, 0, 5, check_defaults, 2);
        session_run(w, &backend); widget_destroy(w);

        char *filter_choices[] = {"Europe/London", "Europe/Belgrade", "America/New_York", "Asia/Tokyo", "Australia/Sydney"};
        w = filter_widget_new("Filter", "Search timezones:", filter_choices, 5, "Type to filter...");
        session_run(w, &backend); widget_destroy(w);

        char *ms_choices[] = {"git", "neovim", "firefox", "alacritty", "tmux", "docker", "podman"};
        w = multiselect_widget_new("Multiselect", "Select packages:", ms_choices, 7, "Search...", 0, 7);
        session_run(w, &backend); widget_destroy(w);

        w = summary_widget_new("Summary", "Line 1\nLine 2\nLine 3\nLine 4\nLine 5\nLine 6\nLine 7\nLine 8\nLine 9\nLine 10", NULL);
        session_run(w, &backend); widget_destroy(w);

        w = file_picker_widget_new("File Picker", getenv("HOME") ? getenv("HOME") : "/", NULL);
        session_run(w, &backend); widget_destroy(w);

        w = text_editor_widget_new("Text Editor", NULL, "Edit me.\nLine 2\nLine 3");
        session_run(w, &backend); widget_destroy(w);

        w = toggle_widget_new("Toggle", "Enable feature?", false);
        session_run(w, &backend); widget_destroy(w);

        cJSON *hub_cats = cJSON_Parse("[{\"label\":\"System\",\"summary_template\":\"Host: {HOSTNAME}\",\"items\":[{\"id\":\"HOSTNAME\",\"label\":\"Hostname\",\"value\":\"artix\",\"widget\":\"input\"},{\"id\":\"INIT\",\"label\":\"Init\",\"value\":\"openrc\",\"widget\":\"menu\",\"choices\":[\"openrc\",\"runit\",\"dinit\",\"s6\"]}]},{\"label\":\"Desktop\",\"summary_template\":\"WM: {WM_DE}\",\"items\":[{\"id\":\"WM_DE\",\"label\":\"WM\",\"value\":\"none\",\"widget\":\"menu\",\"choices\":[\"kde\",\"xfce\",\"sway\",\"hyprland\",\"i3\",\"none\"]}]}]");
        cJSON *hub_acts = cJSON_Parse("[\"Proceed\"]");
        WidgetRequest hub_req = { .widget = "hub", .params = cJSON_CreateObject() };
        cJSON_AddStringToObject(hub_req.params, "title", "Hub");
        cJSON_AddItemToObject(hub_req.params, "categories", hub_cats);
        cJSON_AddItemToObject(hub_req.params, "actions", hub_acts);
        w = widget_registry_create(&hub_req);
        session_run(w, &backend); widget_destroy(w);
        cJSON_Delete(hub_req.params);

        char *headers[] = {"Name", "Size", "Type"};
        char *row1[] = {"firefox", "120MB", "browser"};
        char *row2[] = {"neovim", "8MB", "editor"};
        char *row3[] = {"htop", "2MB", "monitor"};
        char **rows[] = {row1, row2, row3};
        w = table_widget_new("Table", headers, 3, rows, 3);
        session_run(w, &backend); widget_destroy(w);

        w = gauge_widget_new("Gauge", 73, "Progress");
        session_run(w, &backend); widget_destroy(w);

        w = calendar_widget_new("Calendar");
        session_run(w, &backend); widget_destroy(w);

        FormField fields[3] = {
            { .label = "Name", .widget_type = "input", .value = "", .placeholder = "Enter name" },
            { .label = "OS", .widget_type = "menu", .value = "Artix", .placeholder = "" },
            { .label = "Enable", .widget_type = "toggle", .value = "yes", .placeholder = "" },
        };
        w = form_widget_new("Form", fields, 3, "Submit");
        session_run(w, &backend); widget_destroy(w);

        char *radio_choices[] = {"A", "B", "C"};
        w = radio_group_widget_new("Radio", "Pick one:", radio_choices, 3, 0);
        session_run(w, &backend); widget_destroy(w);

        w = range_slider_widget_new("Range Slider", 0, 100, 50, "Volume");
        session_run(w, &backend); widget_destroy(w);

        char *colors[] = {"red", "green", "blue", "yellow", "magenta", "cyan", "white", "black"};
        w = color_picker_widget_new("Color Picker", colors, 8);
        session_run(w, &backend); widget_destroy(w);

        char *ctx_items[] = {"Copy", "Paste", "Delete"};
        w = context_menu_widget_new(ctx_items, 3);
        session_run(w, &backend); widget_destroy(w);

        w = notification_widget_new("Toast notification", 2);
        session_run(w, &backend); widget_destroy(w);

        w = spinner_widget_new("Loading...");
        session_run(w, &backend); widget_destroy(w);

        w = badge_widget_new("FILLY-C");
        session_run(w, &backend); widget_destroy(w);

        char *tab_labels[] = {"General", "Advanced", "About"};
        Widget *tab_children[] = {
            msg_widget_new("General", "General settings"),
            msg_widget_new("Advanced", "Advanced settings"),
            msg_widget_new("About", "FILLY Tabs demo"),
        };
        w = tabs_widget_new("Tabs", tab_labels, 3, tab_children, 3);
        session_run(w, &backend); widget_destroy(w);

        Widget *left = pane_widget_new("Left", "Left pane content");
        Widget *right = pane_widget_new("Right", "Right pane content");
        w = split_panes_widget_new(ORIENT_HORIZONTAL, left, right);
        session_run(w, &backend); widget_destroy(w);

        TreeNode tree_nodes[3] = {
            { .label = "src", .expanded = true, .children = NULL, .child_count = 0 },
            { .label = "include", .expanded = false, .children = NULL, .child_count = 0 },
            { .label = "Makefile", .expanded = false, .children = NULL, .child_count = 0 },
        };
        w = tree_widget_new("Tree", tree_nodes, 3);
        session_run(w, &backend); widget_destroy(w);

        w = msg_widget_new("Done", "All 25 widgets demonstrated.\n\nFILLY-C is operational.");
        session_run(w, &backend); widget_destroy(w);

        terminal_backend_destroy(&t);
        return 0;
    }

    if (strcmp(argv[1], "relay") == 0) {
        load_plugins();
        const char *sock = "/tmp/filly.sock";
        if (argc > 2) sock = argv[2];
        return relay_main(sock);
    }

    print_usage();
    return 1;
}