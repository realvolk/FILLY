#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>
#include <time.h>
#include "core/client.h"
#include "core/widgets/menu.h"
#include "core/widgets/yesno.h"
#include "core/widgets/input.h"
#include "core/widgets/password.h"
#include "core/widgets/checklist.h"
#include "core/widgets/msg.h"
#include "core/widgets/filter.h"
#include "core/widgets/multiselect.h"
#include "core/widgets/file_picker.h"
#include "core/widgets/text_editor.h"
#include "core/widgets/summary.h"
#include "core/widgets/progress.h"
#include "core/widgets/toggle.h"
#include "core/widgets/spinner.h"
#include "core/widgets/separator.h"
#include "core/widgets/disk.h"
#include "core/widgets/table.h"
#include "core/widgets/tree.h"
#include "core/widgets/gauge.h"
#include "core/widgets/calendar.h"
#include "core/widgets/form.h"
#include "core/widgets/tabs.h"
#include "core/widgets/split_panes.h"
#include "core/widgets/context_menu.h"
#include "core/widgets/notification.h"
#include "core/widgets/radio_group.h"
#include "core/widgets/range_slider.h"
#include "core/widgets/color_picker.h"
#include "core/widgets/badge.h"
#include "core/widgets/rich_text.h"
#include "core/widgets/tooltip.h"
#include "core/widgets/hub.h"
#include "protocol/protocol.h"
#include "backend/terminal/terminal.h"
#include "core/widget.h"
#include "core/session.h"
#include "core/theme.h"
#include "core/store.h"
#include "core/config.h"
#include "backend/daemon/daemon.h"
#include "backend/headless/headless.h"
#include "core/relay.h"
#ifdef FILLY_GCORE
#include "backend/gcore/backend.h"
#endif

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
extern void set_insecure_plugins(bool val);
extern void set_use_sandbox(bool val);

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
    for (int i = 0; i < count; i++) { free(choices[i]); }
    free(choices);
    for (int i = 0; i < dcount; i++) { free(defaults[i]); }
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
    for (int i = 0; i < count; i++) { free(choices[i]); }
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
    for (int i = 0; i < count; i++) { free(choices[i]); }
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
    for (int i = 0; i < count; i++) { free(command[i]); }
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
    for (int i = 0; i < hc; i++) { free(headers[i]); }
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
    for (int i = 0; i < count; i++) { free(arr[i]); }
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
    for (int i = 0; i < count; i++) { free(choices[i]); }
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
    for (int i = 0; i < count; i++) { free(colors[i]); }
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
    for (int i = 0; i < count; i++) { free(labels[i]); }
    free(labels); free(children);
    return w;
}

Widget *split_panes_widget_factory(const WidgetRequest *req) {
    cJSON *o = cJSON_GetObjectItem(req->params, "orientation");
    Orientation orient = (o && strcmp(o->valuestring, "vertical") == 0) ? ORIENT_VERTICAL : ORIENT_HORIZONTAL;
    Widget *first = msg_widget_new("Pane 1", "Left / Top content");
    Widget *second = msg_widget_new("Pane 2", "Right / Bottom content");
    return split_panes_widget_new(orient, first, second);
}

Widget *tree_widget_factory(const WidgetRequest *req) {
    cJSON *t = cJSON_GetObjectItem(req->params, "title");
    TreeNode *nodes = malloc(3 * sizeof(TreeNode));
    memset(nodes, 0, 3 * sizeof(TreeNode));
    nodes[0].label = strdup("src");
    nodes[0].expanded = true;
    nodes[1].label = strdup("include");
    nodes[1].expanded = false;
    nodes[2].label = strdup("Makefile");
    nodes[2].expanded = false;
    Widget *w = tree_widget_new(t ? t->valuestring : "Tree", nodes, 3);
    return w;
}

static void print_usage(void) {
    fprintf(stderr, "Usage: filly [command]\n");
    fprintf(stderr, "  daemon [--socket path] [--insecure-plugins] [--sandbox]\n");
    fprintf(stderr, "  oneshot [--input file] [--events file] [--headless] [--gui] [--insecure-plugins]\n");
    fprintf(stderr, "  relay <socket> <json_request>\n");
    fprintf(stderr, "  send [--socket path] [--subscribe keys] [--quit] [--json] <json_request>\n");
    fprintf(stderr, "  build [--file template [--set key=value]...] [<widget> [--<param> <value>...]]\n");
    fprintf(stderr, "  test [--plugins]\n");
}

static KeyCode parse_key_name(const char *name) {
    if (strcmp(name, "UP") == 0) return KEY_UP;
    if (strcmp(name, "DOWN") == 0) return KEY_DOWN;
    if (strcmp(name, "LEFT") == 0) return KEY_LEFT;
    if (strcmp(name, "RIGHT") == 0) return KEY_RIGHT;
    if (strcmp(name, "ENTER") == 0) return KEY_ENTER;
    if (strcmp(name, "ESC") == 0) return KEY_ESC;
    if (strcmp(name, "TAB") == 0) return KEY_TAB;
    if (strcmp(name, "BACKTAB") == 0) return KEY_BACKTAB;
    if (strcmp(name, "BACKSPACE") == 0) return KEY_BACKSPACE;
    if (strcmp(name, "DELETE") == 0) return KEY_DELETE;
    if (strcmp(name, "HOME") == 0) return KEY_HOME;
    if (strcmp(name, "END") == 0) return KEY_END;
    if (strcmp(name, "PAGEUP") == 0) return KEY_PAGEUP;
    if (strcmp(name, "PAGEDOWN") == 0) return KEY_PAGEDOWN;
    if (strcmp(name, "INSERT") == 0) return KEY_INSERT;
    if (strcmp(name, "SPACE") == 0) return KEY_CHAR;
    if (strcmp(name, "F1") == 0) return KEY_F1;
    if (strcmp(name, "F2") == 0) return KEY_F2;
    if (strcmp(name, "F3") == 0) return KEY_F3;
    if (strcmp(name, "F4") == 0) return KEY_F4;
    if (strcmp(name, "F5") == 0) return KEY_F5;
    if (strcmp(name, "F6") == 0) return KEY_F6;
    if (strcmp(name, "F7") == 0) return KEY_F7;
    if (strcmp(name, "F8") == 0) return KEY_F8;
    if (strcmp(name, "F9") == 0) return KEY_F9;
    if (strcmp(name, "F10") == 0) return KEY_F10;
    if (strcmp(name, "F11") == 0) return KEY_F11;
    if (strcmp(name, "F12") == 0) return KEY_F12;
    if (strlen(name) == 1) return KEY_CHAR;
    return KEY_NULL;
}

static void inject_events_from_file(HeadlessBackend *hl, const char *events_path) {
    FILE *f = fopen(events_path, "r");
    if (!f) return;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\n")] = 0;
        if (strlen(line) == 0) continue;
        if (strncmp(line, "KEY:", 4) == 0) {
            char *keyname = line + 4;
            char ch = 0;
            KeyCode code = parse_key_name(keyname);
            if (code == KEY_CHAR) {
                if (strcmp(keyname, "SPACE") == 0) ch = ' ';
                else if (strlen(keyname) == 1) ch = keyname[0];
                else continue;
            }
            headless_inject_key(hl, code, ch);
        } else if (strncmp(line, "TEXT:", 5) == 0) {
            char *text = line + 5;
            for (char *c = text; *c; c++) {
                headless_inject_key(hl, KEY_CHAR, *c);
            }
        } else if (strncmp(line, "WAIT:", 5) == 0) {
            int ms = atoi(line + 5);
            if (ms > 0) poll(NULL, 0, ms);
        }
    }
    fclose(f);
}

static void run_headless_oneshot(const char *input_path, const char *events_path) {
    char *json = NULL;
    if (input_path) {
        FILE *f = fopen(input_path, "r");
        if (!f) { fprintf(stderr, "Cannot open input file\n"); return; }
        fseek(f, 0, SEEK_END);
        long sz = ftell(f);
        rewind(f);
        json = malloc(sz + 1);
        fread(json, 1, sz, f);
        json[sz] = '\0';
        fclose(f);
    } else {
        char buf[65536];
        int n = read(STDIN_FILENO, buf, sizeof(buf) - 1);
        if (n > 0) { buf[n] = '\0'; json = strdup(buf); }
    }
    if (!json || strlen(json) == 0) { fprintf(stderr, "Empty input\n"); return; }

    WidgetRequest *req = widget_request_parse(json);
    if (!req) { fprintf(stderr, "Invalid JSON\n"); free(json); return; }
    free(json);

    Widget *w = widget_registry_create(req);
    if (!w) { fprintf(stderr, "Unknown widget: %s\n", req->widget); widget_request_free(req); return; }

    HeadlessBackend hl;
    headless_backend_init(&hl, 80, 24);
    Backend backend = { .vtable = &headless_vtable, .data = &hl };

    if (events_path) {
        inject_events_from_file(&hl, events_path);
    }

    WidgetResponse resp = session_run(w, &backend);
    char *out = widget_response_to_json(&resp);
    printf("%s\n", out);
    free(out);
    widget_destroy(w);
    widget_request_free(req);
    headless_backend_destroy(&hl);
}

static int cmd_send(int argc, char **argv) {
    const char *socket_path = NULL;
    bool subscribe = false;
    bool quit = false;
    bool json_output = false;
    char *request = NULL;

    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--socket") == 0 && i+1 < argc) {
            socket_path = argv[++i];
        } else if (strcmp(argv[i], "--subscribe") == 0 && i+1 < argc) {
            subscribe = true;
            request = argv[++i];
        } else if (strcmp(argv[i], "--quit") == 0) {
            quit = true;
        } else if (strcmp(argv[i], "--json") == 0) {
            json_output = true;
        } else if (!request) {
            request = argv[i];
        }
    }

    if (!socket_path) {
        const char *xdg = getenv("XDG_RUNTIME_DIR");
        static char default_socket[256];
        if (xdg) snprintf(default_socket, sizeof(default_socket), "%s/filly.sock", xdg);
        else strcpy(default_socket, "/tmp/filly.sock");
        socket_path = default_socket;
    }

    FillyClient *client = filly_client_connect(socket_path);
    if (!client) { fprintf(stderr, "Cannot connect to daemon\n"); return 1; }

    if (quit) {
        filly_client_send_quit(client);
        filly_client_disconnect(client);
        return 0;
    }

    if (subscribe) {
        char *keys = strdup(request);
        char sub[1024];
        char *p = sub;
        p += snprintf(p, sizeof(sub) - (p - sub), "{\"type\":\"subscribe\",\"keys\":[");
        char *token = strtok(keys, ",");
        bool first = true;
        while (token) {
            if (!first) p += snprintf(p, sizeof(sub) - (p - sub), ",");
            p += snprintf(p, sizeof(sub) - (p - sub), "\"%s\"", token);
            first = false;
            token = strtok(NULL, ",");
        }
        p += snprintf(p, sizeof(sub) - (p - sub), "]}\n");
        free(keys);
        filly_client_send_request(client, sub);
        filly_client_disconnect(client);
        return 0;
    }

    if (!request) { fprintf(stderr, "No request provided\n"); filly_client_disconnect(client); return 1; }

    filly_client_send_request(client, request);
    cJSON *result = NULL;
    bool cancelled = false;
    if (filly_client_get_response(client, &result, &cancelled) < 0) {
        filly_client_disconnect(client);
        return 1;
    }
    if (json_output) {
        char *json_str = cJSON_PrintUnformatted(result);
        printf("%s\n", json_str);
        free(json_str);
    } else {
        if (result && result->valuestring) printf("%s\n", result->valuestring);
        else if (result && (result->type == cJSON_True || result->type == cJSON_False)) printf("%s\n", result->type == cJSON_True ? "true" : "false");
        else if (result && result->type == cJSON_Number) printf("%d\n", result->valueint);
        else if (result) {
            char *s = cJSON_PrintUnformatted(result);
            printf("%s\n", s);
            free(s);
        }
    }
    filly_client_disconnect(client);
    return cancelled ? 1 : 0;
}

static int cmd_build(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: filly build [--file <template> [--set key=value]...] [<widget> [--<param> <value>...]]\n");
        return 1;
    }

    if (strcmp(argv[2], "--file") == 0) {
        if (argc < 4) { fprintf(stderr, "Missing template file\n"); return 1; }
        const char *file = argv[3];
        int set_count = 0;
        char *set_keys[64];
        char *set_vals[64];
        int i = 4;
        while (i < argc) {
            if (strcmp(argv[i], "--set") == 0 && i+1 < argc) {
                char *arg = argv[++i];
                char *eq = strchr(arg, '=');
                if (eq) {
                    *eq = '\0';
                    set_keys[set_count] = arg;
                    set_vals[set_count] = eq+1;
                    set_count++;
                }
            } else break;
            i++;
        }
        FILE *fp = fopen(file, "r");
        if (!fp) { fprintf(stderr, "Cannot open %s\n", file); return 1; }
        fseek(fp, 0, SEEK_END);
        long sz = ftell(fp); rewind(fp);
        char *buf = malloc(sz+1);
        fread(buf, 1, sz, fp);
        buf[sz] = '\0';
        fclose(fp);
        char *result = strdup(buf);
        free(buf);
        for (int k = 0; k < set_count; k++) {
            char placeholder[256];
            snprintf(placeholder, sizeof(placeholder), "${%s}", set_keys[k]);
            char *pos = result;
            while ((pos = strstr(pos, placeholder))) {
                size_t plen = strlen(placeholder);
                size_t vlen = strlen(set_vals[k]);
                char *newres = malloc(strlen(result) - plen + vlen + 1);
                memcpy(newres, result, pos - result);
                memcpy(newres + (pos - result), set_vals[k], vlen);
                strcpy(newres + (pos - result) + vlen, pos + plen);
                free(result);
                result = newres;
                pos = newres + (pos - result) + vlen;
            }
            snprintf(placeholder, sizeof(placeholder), "$%s", set_keys[k]);
            pos = result;
            while ((pos = strstr(pos, placeholder))) {
                size_t plen = strlen(placeholder);
                size_t vlen = strlen(set_vals[k]);
                char *newres = malloc(strlen(result) - plen + vlen + 1);
                memcpy(newres, result, pos - result);
                memcpy(newres + (pos - result), set_vals[k], vlen);
                strcpy(newres + (pos - result) + vlen, pos + plen);
                free(result);
                result = newres;
                pos = newres + (pos - result) + vlen;
            }
        }
        printf("%s\n", result);
        free(result);
        return 0;
    }

    const char *widget = argv[2];
    cJSON *params = cJSON_CreateObject();
    int i = 3;
    while (i < argc) {
        if (argv[i][0] == '-' && argv[i][1] == '-') {
            const char *key = argv[i] + 2;
            if (strcmp(key, "choice") == 0 || strcmp(key, "header") == 0 || strcmp(key, "field") == 0) {
                i++;
                cJSON *arr = cJSON_GetObjectItem(params, key);
                if (!arr) { arr = cJSON_CreateArray(); cJSON_AddItemToObject(params, key, arr); }
                if (i < argc) cJSON_AddItemToArray(arr, cJSON_CreateString(argv[i]));
            } else if (strcmp(key, "default") == 0 || strcmp(key, "placeholder") == 0 ||
                       strcmp(key, "title") == 0 || strcmp(key, "message") == 0 ||
                       strcmp(key, "label") == 0 || strcmp(key, "start_dir") == 0 ||
                       strcmp(key, "filter") == 0 || strcmp(key, "file") == 0 ||
                       strcmp(key, "content") == 0 || strcmp(key, "submit_label") == 0) {
                if (i+1 < argc) {
                    cJSON_AddStringToObject(params, key, argv[++i]);
                }
            } else if (strcmp(key, "min") == 0 || strcmp(key, "max") == 0 ||
                       strcmp(key, "percent") == 0 || strcmp(key, "value") == 0 ||
                       strcmp(key, "duration") == 0) {
                if (i+1 < argc) cJSON_AddNumberToObject(params, key, atoi(argv[++i]));
            } else if (strcmp(key, "readonly") == 0) {
                cJSON_AddBoolToObject(params, "readonly", 1);
            } else {
                i++;
            }
        } else {
            i++;
        }
    }
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "widget", widget);
    cJSON_AddItemToObject(root, "params", params);
    char *out = cJSON_PrintUnformatted(root);
    printf("%s\n", out);
    free(out);
    cJSON_Delete(root);
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        print_usage();
        return 1;
    }

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--insecure-plugins") == 0) set_insecure_plugins(true);
    }

    register_builtin_widgets();
    load_plugins();

    g_active_theme = theme_load("themes/forge.json");
    if (!g_active_theme) g_active_theme = theme_load_directory("themes");
    if (!g_active_theme) g_active_theme = theme_default();
    theme_merge_user_override(g_active_theme);

    if (strcmp(argv[1], "send") == 0) return cmd_send(argc, argv);
    if (strcmp(argv[1], "build") == 0) return cmd_build(argc, argv);

    if (strcmp(argv[1], "daemon") == 0) {
        const char *socket = NULL;
        bool sandbox = false;
        for (int i = 2; i < argc; i++) {
            if (strcmp(argv[i], "--socket") == 0 && i + 1 < argc) socket = argv[++i];
            else if (strcmp(argv[i], "--sandbox") == 0) sandbox = true;
        }
        if (sandbox) set_use_sandbox(true);
        return daemon_run(socket) ? 0 : 1;
    }

    if (strcmp(argv[1], "relay") == 0) {
        if (argc < 4) {
            fprintf(stderr, "Usage: filly relay <socket> <json_request>\n");
            return 1;
        }
        return relay_run(argv[2], argv[3]);
    }

    if (strcmp(argv[1], "oneshot") == 0) {
        const char *input = NULL;
        const char *events = NULL;
        bool headless = false;
        bool gui = false;
        for (int i = 2; i < argc; i++) {
            if (strcmp(argv[i], "--input") == 0 && i + 1 < argc) input = argv[++i];
            else if (strcmp(argv[i], "--events") == 0 && i + 1 < argc) events = argv[++i];
            else if (strcmp(argv[i], "--headless") == 0) headless = true;
            else if (strcmp(argv[i], "--gui") == 0) gui = true;
        }

        if (headless) {
            run_headless_oneshot(input, events);
            return 0;
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

        if (gui) {
#ifdef FILLY_GCORE
            GCoreBackend g;
            if (!gcore_backend_init(&g, GCORE_DRM, NULL)) {
                fprintf(stderr, "Failed to init GUI backend\n");
                widget_destroy(w); widget_request_free(req); return 1;
            }
            Backend backend_gui = { .vtable = &gcore_vtable, .data = &g };
            WidgetResponse resp = session_run(w, &backend_gui);
            char *out = widget_response_to_json(&resp);
            printf("\n%s\n", out);
            free(out);
            gcore_backend_destroy(&g);
#else
            fprintf(stderr, "GUI backend not compiled (build with 'make filly-gui')\n");
            return 1;
#endif
        } else {
            TerminalBackend t;
            if (!terminal_backend_init(&t, NULL)) {
                fprintf(stderr, "No terminal available, use --headless\n");
                widget_destroy(w); widget_request_free(req); return 1;
            }
            Backend backend = { .vtable = &terminal_vtable, .data = &t };
            WidgetResponse resp = session_run(w, &backend);
            terminal_backend_destroy(&t);

            char *out = widget_response_to_json(&resp);
            fprintf(stderr, "\r%s\n", out);
            free(out);
            widget_destroy(w);
            widget_request_free(req);
            return 0;
        }
    }
    if (strcmp(argv[1], "test") == 0) {
        bool test_plugins = false;
        for (int i = 2; i < argc; i++) {
            if (strcmp(argv[i], "--plugins") == 0) test_plugins = true;
        }
        printf("{\"status\":\"ok\",\"builtin_widgets\":33,\"plugins_loaded\":%s}\n",
               test_plugins ? "true" : "false");
        return 0;
    }

    print_usage();
    return 1;
}