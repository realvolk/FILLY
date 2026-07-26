#include "core/widget.h"
#include "protocol/protocol.h"
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
#include "core/widgets/terminal_emulator.h"
#include "core/widgets/widget_builder.h"
#include "core/widgets/macro_recorder.h"
#include "cJSON.h"
#include <stdlib.h>
#include <string.h>

typedef Widget *(*RawCtor)(void **args);

static void *extract_param(cJSON *params, const ParamDesc *p) {
    cJSON *v = cJSON_GetObjectItem(params, p->name);
    switch (p->type) {
    case P_STR:  return (v && v->valuestring) ? (void *)v->valuestring : (void *)p->def_str;
    case P_INT:  return (void *)(intptr_t)(v ? v->valueint : p->def_int);
    case P_BOOL: return (void *)(intptr_t)(v ? v->valueint : p->def_int);
    case P_JSON: return v;
    case P_STRS: {
        if (!v) return NULL;
        int n = cJSON_GetArraySize(v);
        char **arr = malloc((n + 1) * sizeof(char *));
        for (int i = 0; i < n; i++) {
            cJSON *item = cJSON_GetArrayItem(v, i);
            arr[i] = item && item->valuestring ? strdup(item->valuestring) : strdup("");
        }
        arr[n] = NULL;
        return arr;
    }
    }
    return NULL;
}

static Widget *generic_factory(const WidgetRequest *req, RawCtor ctor, const ParamDesc *params, int param_count) {
    void *args[16] = {0};
    for (int i = 0; i < param_count && i < 16; i++)
        args[i] = extract_param(req->params, &params[i]);
    return ctor(args);
}

static int count_str_array(char **a) { int n = 0; if (a) while (a[n]) n++; return n; }

static Widget *menu_ctor(void **a) {
    char **choices = (char **)a[2]; int cnt = count_str_array(choices); char *def = (char *)a[4]; int def_idx = 0;
    if (def && choices) for (int i = 0; i < cnt; i++) if (!strcmp(choices[i], def)) { def_idx = i; break; }
    return menu_widget_new((char *)a[0], (char *)a[1], choices, cnt, def_idx);
}
static const ParamDesc menu_params[] = {{"title",P_STR,"",0},{"message",P_STR,"",0},{"choices",P_STRS,NULL,0},{"count",P_INT,NULL,0},{"default",P_STR,"",0}};

static Widget *yesno_ctor(void **a) { return yesno_widget_new((char *)a[0], (char *)a[1], (int)(intptr_t)a[2]); }
static const ParamDesc yesno_params[] = {{"title",P_STR,"",0},{"message",P_STR,"",0},{"default",P_BOOL,NULL,1}};

static Widget *input_ctor(void **a) {
    Widget *w = input_widget_new((char *)a[0], (char *)a[1], (char *)a[2], (char *)a[3], (char *)a[4]);
    const char *vs = (const char *)a[5]; if (vs && vs[0]) input_widget_set_validation_script(w, vs); return w;
}
static const ParamDesc input_params[] = {{"title",P_STR,"",0},{"message",P_STR,"",0},{"default",P_STR,"",0},{"placeholder",P_STR,"",0},{"validation",P_STR,NULL,0},{"validation_script",P_STR,NULL,0}};

static Widget *password_ctor(void **a) { return password_widget_new((char *)a[0], (char *)a[1], (char *)a[2]); }
static const ParamDesc password_params[] = {{"title",P_STR,"",0},{"message",P_STR,"",0},{"placeholder",P_STR,"",0}};

static Widget *checklist_ctor(void **a) {
    char **choices = (char **)a[2]; int cnt = count_str_array(choices); int min_sel = (int)(intptr_t)a[4]; int max_sel = (int)(intptr_t)a[5];
    char **defs = (char **)a[6]; int defc = count_str_array(defs);
    return checklist_widget_new((char *)a[0], (char *)a[1], choices, cnt, min_sel, max_sel, defs, defc);
}
static const ParamDesc checklist_params[] = {{"title",P_STR,"",0},{"message",P_STR,"",0},{"choices",P_STRS,NULL,0},{"count",P_INT,NULL,0},{"min",P_INT,NULL,0},{"max",P_INT,NULL,0},{"default",P_STRS,NULL,0},{"default_count",P_INT,NULL,0}};

static Widget *msg_ctor(void **a) { return msg_widget_new((char *)a[0], (char *)a[1]); }
static const ParamDesc msg_params[] = {{"title",P_STR,"",0},{"message",P_STR,"",0}};

static Widget *filter_ctor(void **a) { char **choices = (char **)a[2]; return filter_widget_new((char *)a[0], (char *)a[1], choices, count_str_array(choices), (char *)a[4]); }
static const ParamDesc filter_params[] = {{"title",P_STR,"",0},{"message",P_STR,"",0},{"choices",P_STRS,NULL,0},{"count",P_INT,NULL,0},{"placeholder",P_STR,NULL,0}};

static Widget *multiselect_ctor(void **a) {
    char **choices = (char **)a[2]; int cnt = count_str_array(choices);
    return multiselect_widget_new((char *)a[0], (char *)a[1], choices, cnt, (char *)a[4], (int)(intptr_t)a[5], (int)(intptr_t)a[6]);
}
static const ParamDesc multiselect_params[] = {{"title",P_STR,"",0},{"message",P_STR,"",0},{"choices",P_STRS,NULL,0},{"count",P_INT,NULL,0},{"placeholder",P_STR,NULL,0},{"min_sel",P_INT,NULL,0},{"max_sel",P_INT,NULL,0}};

static Widget *file_picker_ctor(void **a) { return file_picker_widget_new((char *)a[0], (char *)a[1], (char *)a[2]); }
static const ParamDesc file_picker_params[] = {{"title",P_STR,"",0},{"start_dir",P_STR,"/",0},{"filter_ext",P_STR,NULL,0}};

static Widget *text_editor_ctor(void **a) { return text_editor_widget_new((char *)a[0], (char *)a[1], (char *)a[2]); }
static const ParamDesc text_editor_params[] = {{"title",P_STR,"",0},{"file_path",P_STR,NULL,0},{"content",P_STR,NULL,0}};

static Widget *summary_ctor(void **a) { return summary_widget_new((char *)a[0], (char *)a[1], (char *)a[2]); }
static const ParamDesc summary_params[] = {{"title",P_STR,"",0},{"message",P_STR,"",0},{"file_path",P_STR,NULL,0}};

static Widget *progress_ctor(void **a) { char **cmd = (char **)a[1]; return progress_widget_new((char *)a[0], cmd, count_str_array(cmd), (char *)a[3]); }
static const ParamDesc progress_params[] = {{"title",P_STR,"",0},{"command",P_STRS,NULL,0},{"cmd_count",P_INT,NULL,0},{"logfile",P_STR,NULL,0}};

static Widget *toggle_ctor(void **a) { return toggle_widget_new((char *)a[0], (char *)a[1], (int)(intptr_t)a[2]); }
static const ParamDesc toggle_params[] = {{"title",P_STR,"",0},{"label",P_STR,"",0},{"default",P_BOOL,NULL,0}};

static Widget *spinner_ctor(void **a) { return spinner_widget_new((char *)a[0]); }
static const ParamDesc spinner_params[] = {{"message",P_STR,"",0}};

static Widget *separator_ctor(void **a) { const char *o = (const char *)a[0]; return separator_widget_new((o && !strcmp(o,"vertical")) ? ORIENT_VERTICAL : ORIENT_HORIZONTAL); }
static const ParamDesc separator_params[] = {{"orientation",P_STR,"horizontal",0}};

static Widget *disk_ctor(void **a) { return disk_widget_new((char *)a[0], (char *)a[1], (cJSON *)a[2], (cJSON *)a[3], (int)(intptr_t)a[4]); }
static const ParamDesc disk_params[] = {{"title",P_STR,"",0},{"disk",P_STR,"",0},{"partitions",P_JSON,NULL,0},{"free_space",P_JSON,NULL,0},{"readonly",P_BOOL,NULL,0}};

static Widget *table_ctor(void **a) {
    char **headers = (char **)a[1]; int hc = count_str_array(headers); cJSON *rows_json = (cJSON *)a[3]; int rc = rows_json ? cJSON_GetArraySize(rows_json) : 0;
    char ***rows = malloc(rc * sizeof(char **));
    for (int i = 0; i < rc; i++) { cJSON *row = cJSON_GetArrayItem(rows_json, i); rows[i] = malloc(hc * sizeof(char *)); for (int j = 0; j < hc; j++) { cJSON *cell = cJSON_GetArrayItem(row, j); rows[i][j] = strdup(cell && cell->valuestring ? cell->valuestring : ""); } }
    Widget *w = table_widget_new((char *)a[0], headers, hc, rows, rc);
    for (int i = 0; i < rc; i++) { for (int j = 0; j < hc; j++) free(rows[i][j]); free(rows[i]); } free(rows); return w;
}
static const ParamDesc table_params[] = {{"title",P_STR,"",0},{"headers",P_STRS,NULL,0},{"header_count",P_INT,NULL,0},{"rows",P_JSON,NULL,0},{"row_count",P_INT,NULL,0}};

static Widget *tree_ctor(void **a) {
    cJSON *nodes_json = (cJSON *)a[1]; int count = nodes_json ? cJSON_GetArraySize(nodes_json) : 0;
    TreeNode *nodes = calloc(count, sizeof(TreeNode));
    for (int i = 0; i < count; i++) {
        cJSON *n = cJSON_GetArrayItem(nodes_json, i);
        cJSON *lbl = cJSON_GetObjectItem(n, "label");
        cJSON *exp = cJSON_GetObjectItem(n, "expanded");
        nodes[i].label = strdup(lbl && lbl->valuestring ? lbl->valuestring : "");
        nodes[i].expanded = exp ? exp->valueint : false;
    }
    Widget *w = tree_widget_new((char *)a[0], nodes, count); return w;
}
static const ParamDesc tree_params[] = {{"title",P_STR,"",0},{"nodes",P_JSON,NULL,0},{"node_count",P_INT,NULL,0}};

static Widget *gauge_ctor(void **a) { return gauge_widget_new((char *)a[0], (int)(intptr_t)a[1], (char *)a[2]); }
static const ParamDesc gauge_params[] = {{"title",P_STR,"",0},{"percent",P_INT,NULL,0},{"label",P_STR,"",0}};

static Widget *calendar_ctor(void **a) { return calendar_widget_new((char *)a[0]); }
static const ParamDesc calendar_params[] = {{"title",P_STR,"",0}};

static Widget *form_ctor(void **a) {
    cJSON *fields_json = (cJSON *)a[1];
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
    }
    Widget *w = form_widget_new((char *)a[0], fields, count, (char *)a[3]);
    for (int i = 0; i < count; i++) {
        free(fields[i].label); free(fields[i].widget_type); free(fields[i].value); free(fields[i].placeholder);
    }
    free(fields);
    return w;
}
static const ParamDesc form_params[] = {{"title",P_STR,"",0},{"fields",P_JSON,NULL,0},{"field_count",P_INT,NULL,0},{"submit_label",P_STR,"Submit",0}};

static Widget *tabs_ctor(void **a) { char **labels = (char **)a[1]; int count = count_str_array(labels); Widget **children = malloc(count * sizeof(Widget *)); for (int i = 0; i < count; i++) children[i] = msg_widget_new(labels[i], ""); Widget *w = tabs_widget_new((char *)a[0], labels, count, children, count); free(children); return w; }
static const ParamDesc tabs_params[] = {{"title",P_STR,"",0},{"tab_labels",P_STRS,NULL,0},{"tab_count",P_INT,NULL,0},{"children",P_JSON,NULL,0},{"child_count",P_INT,NULL,0}};

static Widget *split_panes_ctor(void **a) { const char *o = (const char *)a[0]; return split_panes_widget_new((o && !strcmp(o,"vertical")) ? ORIENT_VERTICAL : ORIENT_HORIZONTAL, msg_widget_new("Pane 1",""), msg_widget_new("Pane 2","")); }
static const ParamDesc split_panes_params[] = {{"orientation",P_STR,"horizontal",0},{"first",P_JSON,NULL,0},{"second",P_JSON,NULL,0}};

static Widget *context_menu_ctor(void **a) { char **items = (char **)a[0]; return context_menu_widget_new(items, count_str_array(items)); }
static const ParamDesc context_menu_params[] = {{"items",P_STRS,NULL,0},{"count",P_INT,NULL,0}};

static Widget *notification_ctor(void **a) { return notification_widget_new((char *)a[0], (int)(intptr_t)a[1]); }
static const ParamDesc notification_params[] = {{"message",P_STR,"",0},{"duration",P_INT,NULL,0}};

static Widget *radio_group_ctor(void **a) { char **choices = (char **)a[2]; return radio_group_widget_new((char *)a[0], (char *)a[1], choices, count_str_array(choices), (int)(intptr_t)a[4]); }
static const ParamDesc radio_group_params[] = {{"title",P_STR,"",0},{"message",P_STR,"",0},{"choices",P_STRS,NULL,0},{"count",P_INT,NULL,0},{"default",P_INT,NULL,0}};

static Widget *range_slider_ctor(void **a) { return range_slider_widget_new((char *)a[0], (int)(intptr_t)a[1], (int)(intptr_t)a[2], (int)(intptr_t)a[3], (char *)a[4]); }
static const ParamDesc range_slider_params[] = {{"title",P_STR,"",0},{"min",P_INT,NULL,0},{"max",P_INT,NULL,0},{"value",P_INT,NULL,0},{"label",P_STR,"",0}};

static Widget *color_picker_ctor(void **a) { char **colors = (char **)a[1]; return color_picker_widget_new((char *)a[0], colors, count_str_array(colors)); }
static const ParamDesc color_picker_params[] = {{"title",P_STR,"",0},{"colors",P_STRS,NULL,0},{"color_count",P_INT,NULL,0}};

static Widget *badge_ctor(void **a) { return badge_widget_new((char *)a[0]); }
static const ParamDesc badge_params[] = {{"text",P_STR,"",0}};

static Widget *rich_text_ctor(void **a) { return rich_text_widget_new((char *)a[0]); }
static const ParamDesc rich_text_params[] = {{"content",P_STR,"",0}};

static Widget *tooltip_ctor(void **a) { return tooltip_widget_new((char *)a[0]); }
static const ParamDesc tooltip_params[] = {{"text",P_STR,"",0}};

static Widget *hub_ctor(void **a) { return hub_widget_new((char *)a[0], (cJSON *)a[1], (cJSON *)a[2]); }
static const ParamDesc hub_params[] = {{"title",P_STR,"",0},{"categories",P_JSON,NULL,0},{"actions",P_JSON,NULL,0}};

static Widget *terminal_emulator_ctor(void **a) { char **cmd = (char **)a[1]; return terminal_emulator_widget_new((char *)a[0], cmd, count_str_array(cmd)); }
static const ParamDesc terminal_emulator_params[] = {{"title",P_STR,"",0},{"command",P_STRS,NULL,0},{"cmd_count",P_INT,NULL,0}};

static Widget *widget_builder_ctor(void **a) { (void)a; return widget_builder_new(); }
static const ParamDesc widget_builder_params[] = {};

static Widget *macro_recorder_ctor(void **a) { (void)a; return macro_recorder_widget_new(); }
static const ParamDesc macro_recorder_params[] = {};

#define DEF_FACTORY(name, ctor, params, count) static Widget *name##_factory(const WidgetRequest *req) { return generic_factory(req, (RawCtor)ctor, params, count); }
DEF_FACTORY(menu,menu_ctor,menu_params,5)
DEF_FACTORY(yesno,yesno_ctor,yesno_params,3)
DEF_FACTORY(input,input_ctor,input_params,6)
DEF_FACTORY(password,password_ctor,password_params,3)
DEF_FACTORY(checklist,checklist_ctor,checklist_params,8)
DEF_FACTORY(msg,msg_ctor,msg_params,2)
DEF_FACTORY(filter,filter_ctor,filter_params,5)
DEF_FACTORY(multiselect,multiselect_ctor,multiselect_params,7)
DEF_FACTORY(file_picker,file_picker_ctor,file_picker_params,3)
DEF_FACTORY(text_editor,text_editor_ctor,text_editor_params,3)
DEF_FACTORY(summary,summary_ctor,summary_params,3)
DEF_FACTORY(progress,progress_ctor,progress_params,4)
DEF_FACTORY(toggle,toggle_ctor,toggle_params,3)
DEF_FACTORY(spinner,spinner_ctor,spinner_params,1)
DEF_FACTORY(separator,separator_ctor,separator_params,1)
DEF_FACTORY(disk,disk_ctor,disk_params,5)
DEF_FACTORY(table,table_ctor,table_params,5)
DEF_FACTORY(tree,tree_ctor,tree_params,3)
DEF_FACTORY(gauge,gauge_ctor,gauge_params,3)
DEF_FACTORY(calendar,calendar_ctor,calendar_params,1)
DEF_FACTORY(form,form_ctor,form_params,4)
DEF_FACTORY(tabs,tabs_ctor,tabs_params,5)
DEF_FACTORY(split_panes,split_panes_ctor,split_panes_params,3)
DEF_FACTORY(context_menu,context_menu_ctor,context_menu_params,2)
DEF_FACTORY(notification,notification_ctor,notification_params,2)
DEF_FACTORY(radio_group,radio_group_ctor,radio_group_params,5)
DEF_FACTORY(range_slider,range_slider_ctor,range_slider_params,5)
DEF_FACTORY(color_picker,color_picker_ctor,color_picker_params,3)
DEF_FACTORY(badge,badge_ctor,badge_params,1)
DEF_FACTORY(rich_text,rich_text_ctor,rich_text_params,1)
DEF_FACTORY(tooltip,tooltip_ctor,tooltip_params,1)
DEF_FACTORY(hub,hub_ctor,hub_params,3)
DEF_FACTORY(terminal_emulator,terminal_emulator_ctor,terminal_emulator_params,3)
DEF_FACTORY(widget_builder,widget_builder_ctor,widget_builder_params,0)
DEF_FACTORY(macro_recorder,macro_recorder_ctor,macro_recorder_params,0)

void register_builtin_widgets(void) {
    widget_registry_register("menu",menu_factory);
    widget_registry_register("yesno",yesno_factory);
    widget_registry_register("input",input_factory);
    widget_registry_register("password",password_factory);
    widget_registry_register("checklist",checklist_factory);
    widget_registry_register("msg",msg_factory);
    widget_registry_register("filter",filter_factory);
    widget_registry_register("multiselect",multiselect_factory);
    widget_registry_register("file_picker",file_picker_factory);
    widget_registry_register("text_editor",text_editor_factory);
    widget_registry_register("summary",summary_factory);
    widget_registry_register("progress",progress_factory);
    widget_registry_register("toggle",toggle_factory);
    widget_registry_register("spinner",spinner_factory);
    widget_registry_register("separator",separator_factory);
    widget_registry_register("disk",disk_factory);
    widget_registry_register("table",table_factory);
    widget_registry_register("tree",tree_factory);
    widget_registry_register("gauge",gauge_factory);
    widget_registry_register("calendar",calendar_factory);
    widget_registry_register("form",form_factory);
    widget_registry_register("tabs",tabs_factory);
    widget_registry_register("text",text_editor_factory);
    widget_registry_register("split_panes",split_panes_factory);
    widget_registry_register("context_menu",context_menu_factory);
    widget_registry_register("notification",notification_factory);
    widget_registry_register("radio_group",radio_group_factory);
    widget_registry_register("range_slider",range_slider_factory);
    widget_registry_register("color_picker",color_picker_factory);
    widget_registry_register("badge",badge_factory);
    widget_registry_register("rich_text",rich_text_factory);
    widget_registry_register("tooltip",tooltip_factory);
    widget_registry_register("hub",hub_factory);
    widget_registry_register("terminal_emulator",terminal_emulator_factory);
    widget_registry_register("widget_builder",widget_builder_factory);
    widget_registry_register("macro_recorder",macro_recorder_factory);
}

const ParamDesc *widget_get_params(const char *widget_type, int *count) {
    if (strcmp(widget_type, "menu") == 0) { *count = 5; return menu_params; }
    if (strcmp(widget_type, "yesno") == 0) { *count = 3; return yesno_params; }
    if (strcmp(widget_type, "input") == 0) { *count = 6; return input_params; }
    if (strcmp(widget_type, "password") == 0) { *count = 3; return password_params; }
    if (strcmp(widget_type, "checklist") == 0) { *count = 8; return checklist_params; }
    if (strcmp(widget_type, "msg") == 0) { *count = 2; return msg_params; }
    if (strcmp(widget_type, "filter") == 0) { *count = 5; return filter_params; }
    if (strcmp(widget_type, "multiselect") == 0) { *count = 7; return multiselect_params; }
    if (strcmp(widget_type, "file_picker") == 0) { *count = 3; return file_picker_params; }
    if (strcmp(widget_type, "text_editor") == 0) { *count = 3; return text_editor_params; }
    if (strcmp(widget_type, "summary") == 0) { *count = 3; return summary_params; }
    if (strcmp(widget_type, "progress") == 0) { *count = 4; return progress_params; }
    if (strcmp(widget_type, "toggle") == 0) { *count = 3; return toggle_params; }
    if (strcmp(widget_type, "spinner") == 0) { *count = 1; return spinner_params; }
    if (strcmp(widget_type, "separator") == 0) { *count = 1; return separator_params; }
    if (strcmp(widget_type, "disk") == 0) { *count = 5; return disk_params; }
    if (strcmp(widget_type, "table") == 0) { *count = 5; return table_params; }
    if (strcmp(widget_type, "tree") == 0) { *count = 3; return tree_params; }
    if (strcmp(widget_type, "gauge") == 0) { *count = 3; return gauge_params; }
    if (strcmp(widget_type, "calendar") == 0) { *count = 1; return calendar_params; }
    if (strcmp(widget_type, "form") == 0) { *count = 4; return form_params; }
    if (strcmp(widget_type, "tabs") == 0) { *count = 5; return tabs_params; }
    if (strcmp(widget_type, "split_panes") == 0) { *count = 3; return split_panes_params; }
    if (strcmp(widget_type, "context_menu") == 0) { *count = 2; return context_menu_params; }
    if (strcmp(widget_type, "notification") == 0) { *count = 2; return notification_params; }
    if (strcmp(widget_type, "radio_group") == 0) { *count = 5; return radio_group_params; }
    if (strcmp(widget_type, "range_slider") == 0) { *count = 5; return range_slider_params; }
    if (strcmp(widget_type, "color_picker") == 0) { *count = 3; return color_picker_params; }
    if (strcmp(widget_type, "badge") == 0) { *count = 1; return badge_params; }
    if (strcmp(widget_type, "rich_text") == 0) { *count = 1; return rich_text_params; }
    if (strcmp(widget_type, "tooltip") == 0) { *count = 1; return tooltip_params; }
    if (strcmp(widget_type, "hub") == 0) { *count = 3; return hub_params; }
    if (strcmp(widget_type, "terminal_emulator") == 0) { *count = 3; return terminal_emulator_params; }
    if (strcmp(widget_type, "widget_builder") == 0) { *count = 0; return widget_builder_params; }
    if (strcmp(widget_type, "macro_recorder") == 0) { *count = 0; return macro_recorder_params; }
    *count = 0;
    return NULL;
}