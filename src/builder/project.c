#include "project.h"
#include "core/widget.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct {
    const char *widget_type;
    PortDef outputs[7];
    int output_count;
    PortDef inputs[5];
    int input_count;
} WidgetPortInfo;

static const WidgetPortInfo port_registry[] = {
    {"input",
        {{0,"on_submit",PORT_TRIGGER,true,"Emitted when Enter is pressed"},
         {1,"on_change",PORT_TRIGGER,true,"Emitted on each keystroke"},
         {2,"value",PORT_STRING,true,"Current text value"},
         {3,"animation_end",PORT_TRIGGER,true,"Fires when an animation completes"}}, 4,
        {{0,"set_value",PORT_STRING,false,"Set the input text"},
         {1,"set_visible",PORT_BOOL,false,"Show or hide this widget"},
         {2,"play_animation",PORT_STRING,false,"Play a named animation"}}, 3},
    {"password",
        {{0,"on_submit",PORT_TRIGGER,true,"Emitted when Enter is pressed"},
         {1,"value",PORT_STRING,true,"Current password value"},
         {2,"animation_end",PORT_TRIGGER,true,"Fires when an animation completes"}}, 3,
        {{0,"set_visible",PORT_BOOL,false,"Show or hide this widget"},
         {1,"play_animation",PORT_STRING,false,"Play a named animation"}}, 2},
    {"toggle",
        {{0,"on_toggle",PORT_TRIGGER,true,"Emitted when toggled"},
         {1,"value",PORT_BOOL,true,"Current on/off state"},
         {2,"animation_end",PORT_TRIGGER,true,"Fires when an animation completes"}}, 3,
        {{0,"set_value",PORT_BOOL,false,"Set toggle state"},
         {1,"set_visible",PORT_BOOL,false,"Show or hide this widget"},
         {2,"play_animation",PORT_STRING,false,"Play a named animation"}}, 3},
    {"checkbox",
        {{0,"on_check",PORT_TRIGGER,true,"Emitted when checked/unchecked"},
         {1,"checked",PORT_BOOL,true,"Current check state"},
         {2,"animation_end",PORT_TRIGGER,true,"Fires when an animation completes"}}, 3,
        {{0,"set_checked",PORT_BOOL,false,"Set check state"},
         {1,"set_visible",PORT_BOOL,false,"Show or hide this widget"},
         {2,"play_animation",PORT_STRING,false,"Play a named animation"}}, 3},
    {"menu",
        {{0,"on_select",PORT_TRIGGER,true,"Emitted on item selection"},
         {1,"value",PORT_STRING,true,"Label of selected item"},
         {2,"index",PORT_INT,true,"Index of selected item"},
         {3,"animation_end",PORT_TRIGGER,true,"Fires when an animation completes"}}, 4,
        {{0,"set_visible",PORT_BOOL,false,"Show or hide this widget"},
         {1,"play_animation",PORT_STRING,false,"Play a named animation"}}, 2},
    {"checklist",
        {{0,"on_change",PORT_TRIGGER,true,"Emitted when selection changes"},
         {1,"selected",PORT_STRING,true,"Comma-separated selected items"},
         {2,"animation_end",PORT_TRIGGER,true,"Fires when an animation completes"}}, 3,
        {{0,"set_visible",PORT_BOOL,false,"Show or hide this widget"},
         {1,"play_animation",PORT_STRING,false,"Play a named animation"}}, 2},
    {"multiselect",
        {{0,"on_change",PORT_TRIGGER,true,"Emitted when selection changes"},
         {1,"selected",PORT_STRING,true,"Comma-separated selected items"},
         {2,"animation_end",PORT_TRIGGER,true,"Fires when an animation completes"}}, 3,
        {{0,"set_visible",PORT_BOOL,false,"Show or hide this widget"},
         {1,"play_animation",PORT_STRING,false,"Play a named animation"}}, 2},
    {"filter",
        {{0,"on_select",PORT_TRIGGER,true,"Emitted on item selection"},
         {1,"value",PORT_STRING,true,"Label of selected item"},
         {2,"animation_end",PORT_TRIGGER,true,"Fires when an animation completes"}}, 3,
        {{0,"set_visible",PORT_BOOL,false,"Show or hide this widget"},
         {1,"play_animation",PORT_STRING,false,"Play a named animation"}}, 2},
    {"radio_group",
        {{0,"on_select",PORT_TRIGGER,true,"Emitted on item selection"},
         {1,"value",PORT_STRING,true,"Label of selected item"},
         {2,"index",PORT_INT,true,"Index of selected item"},
         {3,"animation_end",PORT_TRIGGER,true,"Fires when an animation completes"}}, 4,
        {{0,"set_visible",PORT_BOOL,false,"Show or hide this widget"},
         {1,"play_animation",PORT_STRING,false,"Play a named animation"}}, 2},
    {"calendar",
        {{0,"on_select",PORT_TRIGGER,true,"Emitted when a day is selected"},
         {1,"date",PORT_STRING,true,"Selected date as YYYY-MM-DD"},
         {2,"animation_end",PORT_TRIGGER,true,"Fires when an animation completes"}}, 3,
        {{0,"set_visible",PORT_BOOL,false,"Show or hide this widget"},
         {1,"play_animation",PORT_STRING,false,"Play a named animation"}}, 2},
    {"color_picker",
        {{0,"on_select",PORT_TRIGGER,true,"Emitted when a color is picked"},
         {1,"color",PORT_STRING,true,"Selected color as hex #RRGGBB"},
         {2,"animation_end",PORT_TRIGGER,true,"Fires when an animation completes"}}, 3,
        {{0,"set_visible",PORT_BOOL,false,"Show or hide this widget"},
         {1,"play_animation",PORT_STRING,false,"Play a named animation"}}, 2},
    {"range_slider",
        {{0,"on_change",PORT_TRIGGER,true,"Emitted when slider moves"},
         {1,"value",PORT_INT,true,"Current slider value"},
         {2,"animation_end",PORT_TRIGGER,true,"Fires when an animation completes"}}, 3,
        {{0,"set_value",PORT_INT,false,"Set slider value"},
         {1,"set_visible",PORT_BOOL,false,"Show or hide this widget"},
         {2,"play_animation",PORT_STRING,false,"Play a named animation"}}, 3},
    {"file_picker",
        {{0,"on_select",PORT_TRIGGER,true,"Emitted when a file is picked"},
         {1,"path",PORT_STRING,true,"Selected file path"},
         {2,"animation_end",PORT_TRIGGER,true,"Fires when an animation completes"}}, 3,
        {{0,"set_visible",PORT_BOOL,false,"Show or hide this widget"},
         {1,"play_animation",PORT_STRING,false,"Play a named animation"}}, 2},
    {"text_editor",
        {{0,"on_save",PORT_TRIGGER,true,"Emitted when Ctrl+S is pressed"},
         {1,"content",PORT_STRING,true,"Current text content"},
         {2,"animation_end",PORT_TRIGGER,true,"Fires when an animation completes"}}, 3,
        {{0,"set_content",PORT_STRING,false,"Set the editor content"},
         {1,"set_visible",PORT_BOOL,false,"Show or hide this widget"},
         {2,"play_animation",PORT_STRING,false,"Play a named animation"}}, 3},
    {"form",
        {{0,"on_submit",PORT_TRIGGER,true,"Emitted when form is submitted"},
         {1,"values",PORT_STRING,true,"JSON string of form values"},
         {2,"animation_end",PORT_TRIGGER,true,"Fires when an animation completes"}}, 3,
        {{0,"set_visible",PORT_BOOL,false,"Show or hide this widget"},
         {1,"play_animation",PORT_STRING,false,"Play a named animation"}}, 2},
    {"progress",
        {{0,"on_complete",PORT_TRIGGER,true,"Emitted when progress reaches 100%"},
         {1,"percent",PORT_INT,true,"Current progress percentage"},
         {2,"animation_end",PORT_TRIGGER,true,"Fires when an animation completes"}}, 3,
        {{0,"set_visible",PORT_BOOL,false,"Show or hide this widget"},
         {1,"play_animation",PORT_STRING,false,"Play a named animation"}}, 2},
    {"tabs",
        {{0,"on_tab_change",PORT_TRIGGER,true,"Emitted when active tab changes"},
         {1,"active_tab",PORT_INT,true,"Index of active tab"},
         {2,"animation_end",PORT_TRIGGER,true,"Fires when an animation completes"}}, 3,
        {{0,"set_active_tab",PORT_INT,false,"Switch to a tab by index"},
         {1,"set_visible",PORT_BOOL,false,"Show or hide this widget"},
         {2,"play_animation",PORT_STRING,false,"Play a named animation"}}, 3},
    {"split_panes",
        {{0,"on_resize",PORT_TRIGGER,true,"Emitted when split position changes"},
         {1,"split_pos",PORT_INT,true,"Current split position in pixels"},
         {2,"animation_end",PORT_TRIGGER,true,"Fires when an animation completes"}}, 3,
        {{0,"set_visible",PORT_BOOL,false,"Show or hide this widget"},
         {1,"play_animation",PORT_STRING,false,"Play a named animation"}}, 2},
    {"table",
        {{0,"on_select",PORT_TRIGGER,true,"Emitted when a cell is selected"},
         {1,"value",PORT_STRING,true,"Selected cell value"},
         {2,"row",PORT_INT,true,"Selected row index"},
         {3,"col",PORT_INT,true,"Selected column index"},
         {4,"animation_end",PORT_TRIGGER,true,"Fires when an animation completes"}}, 5,
        {{0,"set_visible",PORT_BOOL,false,"Show or hide this widget"},
         {1,"play_animation",PORT_STRING,false,"Play a named animation"}}, 2},
    {"tree",
        {{0,"on_select",PORT_TRIGGER,true,"Emitted when a node is selected"},
         {1,"path",PORT_STRING,true,"Selected node path"},
         {2,"animation_end",PORT_TRIGGER,true,"Fires when an animation completes"}}, 3,
        {{0,"set_visible",PORT_BOOL,false,"Show or hide this widget"},
         {1,"play_animation",PORT_STRING,false,"Play a named animation"}}, 2},
    {"gauge",
        {{0,"percent",PORT_INT,true,"Current gauge percentage"},
         {1,"animation_end",PORT_TRIGGER,true,"Fires when an animation completes"}}, 2,
        {{0,"set_value",PORT_INT,false,"Set gauge percentage"},
         {1,"set_visible",PORT_BOOL,false,"Show or hide this widget"},
         {2,"play_animation",PORT_STRING,false,"Play a named animation"}}, 3},
    {"spinner",
        {{0,"animation_end",PORT_TRIGGER,true,"Fires when an animation completes"}}, 1,
        {{0,"set_visible",PORT_BOOL,false,"Show or hide this widget"},
         {1,"play_animation",PORT_STRING,false,"Play a named animation"}}, 2},
    {"separator",
        {{0,"animation_end",PORT_TRIGGER,true,"Fires when an animation completes"}}, 1,
        {{0,"set_visible",PORT_BOOL,false,"Show or hide this widget"},
         {1,"play_animation",PORT_STRING,false,"Play a named animation"}}, 2},
    {"badge",
        {{0,"animation_end",PORT_TRIGGER,true,"Fires when an animation completes"}}, 1,
        {{0,"set_text",PORT_STRING,false,"Set badge text"},
         {1,"set_visible",PORT_BOOL,false,"Show or hide this widget"},
         {2,"play_animation",PORT_STRING,false,"Play a named animation"}}, 3},
    {"msg",
        {{0,"animation_end",PORT_TRIGGER,true,"Fires when an animation completes"}}, 1,
        {{0,"set_visible",PORT_BOOL,false,"Show or hide this widget"},
         {1,"play_animation",PORT_STRING,false,"Play a named animation"}}, 2},
    {"summary",
        {{0,"animation_end",PORT_TRIGGER,true,"Fires when an animation completes"}}, 1,
        {{0,"set_visible",PORT_BOOL,false,"Show or hide this widget"},
         {1,"play_animation",PORT_STRING,false,"Play a named animation"}}, 2},
    {"notification",
        {{0,"on_dismiss",PORT_TRIGGER,true,"Emitted when notification is dismissed"},
         {1,"animation_end",PORT_TRIGGER,true,"Fires when an animation completes"}}, 2,
        {{0,"set_visible",PORT_BOOL,false,"Show or hide this widget"},
         {1,"play_animation",PORT_STRING,false,"Play a named animation"}}, 2},
    {"tooltip",
        {{0,"animation_end",PORT_TRIGGER,true,"Fires when an animation completes"}}, 1,
        {{0,"set_text",PORT_STRING,false,"Set tooltip text"},
         {1,"set_visible",PORT_BOOL,false,"Show or hide this widget"},
         {2,"play_animation",PORT_STRING,false,"Play a named animation"}}, 3},
    {"rich_text",
        {{0,"animation_end",PORT_TRIGGER,true,"Fires when an animation completes"}}, 1,
        {{0,"set_content",PORT_STRING,false,"Set the rich text content"},
         {1,"set_visible",PORT_BOOL,false,"Show or hide this widget"},
         {2,"play_animation",PORT_STRING,false,"Play a named animation"}}, 3},
    {"context_menu",
        {{0,"on_select",PORT_TRIGGER,true,"Emitted when an item is selected"},
         {1,"value",PORT_STRING,true,"Label of selected item"},
         {2,"animation_end",PORT_TRIGGER,true,"Fires when an animation completes"}}, 3,
        {{0,"set_visible",PORT_BOOL,false,"Show or hide this widget"},
         {1,"play_animation",PORT_STRING,false,"Play a named animation"}}, 2},
    {"disk",
        {{0,"on_change",PORT_TRIGGER,true,"Emitted when partitions change"},
         {1,"partitions",PORT_STRING,true,"JSON string of partition data"},
         {2,"animation_end",PORT_TRIGGER,true,"Fires when an animation completes"}}, 3,
        {{0,"set_visible",PORT_BOOL,false,"Show or hide this widget"},
         {1,"play_animation",PORT_STRING,false,"Play a named animation"}}, 2},
    {"hub",
        {{0,"on_navigate",PORT_TRIGGER,true,"Emitted when section changes"},
         {1,"active_section",PORT_STRING,true,"Name of active section"},
         {2,"animation_end",PORT_TRIGGER,true,"Fires when an animation completes"}}, 3,
        {{0,"set_visible",PORT_BOOL,false,"Show or hide this widget"},
         {1,"play_animation",PORT_STRING,false,"Play a named animation"}}, 2},
    {"terminal_emulator",
        {{0,"on_exit",PORT_TRIGGER,true,"Emitted when the child process exits"},
         {1,"exit_code",PORT_INT,true,"Exit code of the child process"},
         {2,"animation_end",PORT_TRIGGER,true,"Fires when an animation completes"}}, 3,
        {{0,"set_visible",PORT_BOOL,false,"Show or hide this widget"},
         {1,"play_animation",PORT_STRING,false,"Play a named animation"}}, 2},
    {"widget_builder",
        {{0,"on_export",PORT_TRIGGER,true,"Emitted when layout is exported"},
         {1,"layout_json",PORT_STRING,true,"JSON string of the composed layout"},
         {2,"animation_end",PORT_TRIGGER,true,"Fires when an animation completes"}}, 3,
        {{0,"set_visible",PORT_BOOL,false,"Show or hide this widget"},
         {1,"play_animation",PORT_STRING,false,"Play a named animation"}}, 2},
    {"macro_recorder",
        {{0,"on_save",PORT_TRIGGER,true,"Emitted when a recording is saved"},
         {1,"recording_path",PORT_STRING,true,"Path to the saved recording"},
         {2,"animation_end",PORT_TRIGGER,true,"Fires when an animation completes"}}, 3,
        {{0,"set_visible",PORT_BOOL,false,"Show or hide this widget"},
         {1,"play_animation",PORT_STRING,false,"Play a named animation"}}, 2},
    {"yesno",
        {{0,"on_yes",PORT_TRIGGER,true,"Emitted when Yes is selected"},
         {1,"on_no",PORT_TRIGGER,true,"Emitted when No is selected"},
         {2,"choice",PORT_BOOL,true,"True if Yes was chosen"},
         {3,"animation_end",PORT_TRIGGER,true,"Fires when an animation completes"}}, 4,
        {{0,"set_visible",PORT_BOOL,false,"Show or hide this widget"},
         {1,"play_animation",PORT_STRING,false,"Play a named animation"}}, 2},
    {"button",
        {{0,"on_click",PORT_TRIGGER,true,"Emitted when the button is clicked/pressed"},
         {1,"animation_end",PORT_TRIGGER,true,"Fires when an animation completes"}}, 2,
        {{0,"set_visible",PORT_BOOL,false,"Show or hide this widget"},
         {1,"play_animation",PORT_STRING,false,"Play a named animation"}}, 2},
    {"list",
        {{0,"on_select",PORT_TRIGGER,true,"Emitted on item selection"},
         {1,"value",PORT_STRING,true,"Label of selected item"},
         {2,"index",PORT_INT,true,"Index of selected item"},
         {3,"animation_end",PORT_TRIGGER,true,"Fires when an animation completes"}}, 4,
        {{0,"set_visible",PORT_BOOL,false,"Show or hide this widget"},
         {1,"play_animation",PORT_STRING,false,"Play a named animation"}}, 2},
};

static const int port_registry_count = sizeof(port_registry) / sizeof(port_registry[0]);

int project_get_widget_ports(const char *widget_type, PortDef **ports) {
    for (int i = 0; i < port_registry_count; i++) {
        if (strcmp(port_registry[i].widget_type, widget_type) == 0) {
            int total = port_registry[i].output_count + port_registry[i].input_count;
            PortDef *out = malloc(total * sizeof(PortDef));
            int idx = 0;
            for (int j = 0; j < port_registry[i].output_count; j++) {
                out[idx] = port_registry[i].outputs[j];
                out[idx].is_output = true;
                idx++;
            }
            for (int j = 0; j < port_registry[i].input_count; j++) {
                out[idx] = port_registry[i].inputs[j];
                out[idx].is_output = false;
                idx++;
            }
            *ports = out;
            return total;
        }
    }
    PortDef *fallback = malloc(sizeof(PortDef));
    fallback[0] = (PortDef){0, "set_visible", PORT_BOOL, false, "Show or hide this widget"};
    *ports = fallback;
    return 1;
}

BuilderProject *project_new(const char *name, int root_w, int root_h) {
    BuilderProject *p = calloc(1, sizeof(BuilderProject));
    p->project_name = strdup(name ? name : "untitled");
    p->root_width = root_w > 0 ? root_w : 800;
    p->root_height = root_h > 0 ? root_h : 600;
    p->next_item_id = 1;
    p->filly_version = 1;
    p->undo = undo_stack_new(200);
    p->tui.min_terminal_width = 80;
    p->tui.min_terminal_height = 24;
    p->tui.show_tui_overlay = false;
    return p;
}

void project_free(BuilderProject *p) {
    if (!p) return;
    free(p->project_name);
    free(p->file_path);
    free(p->author);
    free(p->description);
    free(p->tui.fallback_font);
    for (int i = 0; i < p->item_count; i++) {
        free(p->items[i].widget_type);
        free(p->items[i].instance_name);
        if (p->items[i].params) cJSON_Delete(p->items[i].params);
    }
    free(p->items);
    for (int i = 0; i < p->node_count; i++) {
        free(p->nodes[i].label);
        free(p->nodes[i].store_key);
        free(p->nodes[i].fil_script);
        free(p->nodes[i].ports);
    }
    free(p->nodes);
    for (int i = 0; i < p->edge_count; i++) {
        free(p->edges[i].condition);
        free(p->edges[i].transform);
    }
    free(p->edges);
    for (int i = 0; i < p->keymap_count; i++) {
        free(p->keymaps[i].key);
        free(p->keymaps[i].action);
    }
    free(p->keymaps);
    for (int i = 0; i < p->store_var_count; i++) {
        free(p->store_vars[i].key);
        free(p->store_vars[i].initial_value);
        free(p->store_vars[i].type);
    }
    free(p->store_vars);
    if (p->undo) undo_stack_free(p->undo);
    free(p);
}

bool project_save(BuilderProject *p, const char *path) {
    if (!p || !path) return false;
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "version", 1);
    cJSON_AddNumberToObject(root, "filly_version", p->filly_version);
    cJSON_AddStringToObject(root, "name", p->project_name);
    cJSON_AddStringToObject(root, "author", p->author ? p->author : "");
    cJSON_AddStringToObject(root, "description", p->description ? p->description : "");

    cJSON *r = cJSON_CreateObject();
    cJSON_AddNumberToObject(r, "w", p->root_width);
    cJSON_AddNumberToObject(r, "h", p->root_height);
    cJSON_AddItemToObject(root, "root", r);

    cJSON *items_arr = cJSON_CreateArray();
    for (int i = 0; i < p->item_count; i++) {
        CanvasItem *item = &p->items[i];
        cJSON *o = cJSON_CreateObject();
        cJSON_AddNumberToObject(o, "id", item->id);
        cJSON_AddStringToObject(o, "type", item->widget_type);
        cJSON_AddStringToObject(o, "name", item->instance_name);
        cJSON *rect = cJSON_CreateObject();
        cJSON_AddNumberToObject(rect, "x", item->rect.x);
        cJSON_AddNumberToObject(rect, "y", item->rect.y);
        cJSON_AddNumberToObject(rect, "w", item->rect.w);
        cJSON_AddNumberToObject(rect, "h", item->rect.h);
        cJSON_AddItemToObject(o, "rect", rect);
        if (item->params) cJSON_AddItemToObject(o, "params", cJSON_Duplicate(item->params, 1));
        cJSON_AddNumberToObject(o, "parent", item->parent_id);
        cJSON_AddNumberToObject(o, "tab_index", item->tab_index);
        cJSON_AddBoolToObject(o, "locked", item->locked);
        cJSON_AddBoolToObject(o, "visible", item->visible);
        cJSON_AddItemToArray(items_arr, o);
    }
    cJSON_AddItemToObject(root, "items", items_arr);

    cJSON *nodes_arr = cJSON_CreateArray();
    for (int i = 0; i < p->node_count; i++) {
        GraphNode *n = &p->nodes[i];
        cJSON *o = cJSON_CreateObject();
        cJSON_AddNumberToObject(o, "id", n->id);
        cJSON_AddNumberToObject(o, "type", n->type);
        cJSON_AddStringToObject(o, "label", n->label);
        if (n->widget_id >= 0) cJSON_AddNumberToObject(o, "widget_id", n->widget_id);
        if (n->store_key) cJSON_AddStringToObject(o, "store_key", n->store_key);
        if (n->fil_script) cJSON_AddStringToObject(o, "fil_script", n->fil_script);
        cJSON *ports_arr = cJSON_CreateArray();
        for (int j = 0; j < n->port_count; j++) {
            cJSON *po = cJSON_CreateObject();
            cJSON_AddNumberToObject(po, "id", n->ports[j].id);
            cJSON_AddStringToObject(po, "name", n->ports[j].name);
            cJSON_AddNumberToObject(po, "type", n->ports[j].type);
            cJSON_AddBoolToObject(po, "out", n->ports[j].is_output);
            if (n->ports[j].description) cJSON_AddStringToObject(po, "desc", n->ports[j].description);
            cJSON_AddItemToArray(ports_arr, po);
        }
        cJSON_AddItemToObject(o, "ports", ports_arr);
        cJSON_AddItemToArray(nodes_arr, o);
    }
    cJSON_AddItemToObject(root, "nodes", nodes_arr);

    cJSON *edges_arr = cJSON_CreateArray();
    for (int i = 0; i < p->edge_count; i++) {
        cJSON *o = cJSON_CreateObject();
        cJSON_AddNumberToObject(o, "from_node", p->edges[i].from_node);
        cJSON_AddNumberToObject(o, "from_port", p->edges[i].from_port);
        cJSON_AddNumberToObject(o, "to_node", p->edges[i].to_node);
        cJSON_AddNumberToObject(o, "to_port", p->edges[i].to_port);
        if (p->edges[i].condition) cJSON_AddStringToObject(o, "condition", p->edges[i].condition);
        if (p->edges[i].transform) cJSON_AddStringToObject(o, "transform", p->edges[i].transform);
        cJSON_AddItemToArray(edges_arr, o);
    }
    cJSON_AddItemToObject(root, "edges", edges_arr);

    cJSON *km_arr = cJSON_CreateArray();
    for (int i = 0; i < p->keymap_count; i++) {
        cJSON *o = cJSON_CreateObject();
        cJSON_AddNumberToObject(o, "scope", p->keymaps[i].scope);
        cJSON_AddNumberToObject(o, "widget_id", p->keymaps[i].widget_id);
        cJSON_AddStringToObject(o, "key", p->keymaps[i].key);
        cJSON_AddStringToObject(o, "action", p->keymaps[i].action);
        cJSON_AddItemToArray(km_arr, o);
    }
    cJSON_AddItemToObject(root, "keymaps", km_arr);

    cJSON *sv_arr = cJSON_CreateArray();
    for (int i = 0; i < p->store_var_count; i++) {
        cJSON *o = cJSON_CreateObject();
        cJSON_AddStringToObject(o, "key", p->store_vars[i].key);
        cJSON_AddStringToObject(o, "initial", p->store_vars[i].initial_value);
        cJSON_AddStringToObject(o, "type", p->store_vars[i].type);
        cJSON_AddItemToArray(sv_arr, o);
    }
    cJSON_AddItemToObject(root, "store_vars", sv_arr);

    cJSON *tui = cJSON_CreateObject();
    cJSON_AddBoolToObject(tui, "show_tab_order", p->tui.show_tui_overlay);
    cJSON_AddNumberToObject(tui, "min_term_w", p->tui.min_terminal_width);
    cJSON_AddNumberToObject(tui, "min_term_h", p->tui.min_terminal_height);
    if (p->tui.fallback_font) cJSON_AddStringToObject(tui, "fallback_font", p->tui.fallback_font);
    cJSON_AddItemToObject(root, "tui", tui);

    char *json = cJSON_PrintUnformatted(root);
    FILE *f = fopen(path, "w");
    if (!f) { cJSON_Delete(root); free(json); return false; }
    fprintf(f, "%s\n", json);
    fclose(f);
    free(json);
    cJSON_Delete(root);
    return true;
}

BuilderProject *project_load(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    rewind(f);
    char *buf = malloc(sz + 1);
    fread(buf, 1, sz, f);
    buf[sz] = '\0';
    fclose(f);
    cJSON *root = cJSON_Parse(buf);
    free(buf);
    if (!root) return NULL;

    BuilderProject *p = calloc(1, sizeof(BuilderProject));
    cJSON *name = cJSON_GetObjectItem(root, "name");
    p->project_name = strdup(name && name->valuestring ? name->valuestring : "untitled");
    p->file_path = strdup(path);
    cJSON *auth = cJSON_GetObjectItem(root, "author");
    if (auth && auth->valuestring) p->author = strdup(auth->valuestring);
    cJSON *desc = cJSON_GetObjectItem(root, "description");
    if (desc && desc->valuestring) p->description = strdup(desc->valuestring);
    cJSON *ver = cJSON_GetObjectItem(root, "filly_version");
    p->filly_version = ver ? ver->valueint : 1;

    cJSON *r = cJSON_GetObjectItem(root, "root");
    if (r) {
        cJSON *rw = cJSON_GetObjectItem(r, "w");
        cJSON *rh = cJSON_GetObjectItem(r, "h");
        p->root_width = rw ? rw->valueint : 800;
        p->root_height = rh ? rh->valueint : 600;
    }

    cJSON *items_arr = cJSON_GetObjectItem(root, "items");
    if (items_arr && cJSON_IsArray(items_arr)) {
        p->item_count = cJSON_GetArraySize(items_arr);
        p->items = calloc(p->item_count, sizeof(CanvasItem));
        for (int i = 0; i < p->item_count; i++) {
            cJSON *o = cJSON_GetArrayItem(items_arr, i);
            CanvasItem *item = &p->items[i];
            item->id = cJSON_GetObjectItem(o, "id")->valueint;
            cJSON *wt = cJSON_GetObjectItem(o, "type");
            item->widget_type = strdup(wt && wt->valuestring ? wt->valuestring : "");
            cJSON *nm = cJSON_GetObjectItem(o, "name");
            item->instance_name = strdup(nm && nm->valuestring ? nm->valuestring : "");
            cJSON *rect = cJSON_GetObjectItem(o, "rect");
            if (rect) {
                item->rect.x = cJSON_GetObjectItem(rect, "x")->valueint;
                item->rect.y = cJSON_GetObjectItem(rect, "y")->valueint;
                item->rect.w = cJSON_GetObjectItem(rect, "w")->valueint;
                item->rect.h = cJSON_GetObjectItem(rect, "h")->valueint;
            }
            cJSON *params = cJSON_GetObjectItem(o, "params");
            if (params) item->params = cJSON_Duplicate(params, 1);
            else item->params = cJSON_CreateObject();
            cJSON *par = cJSON_GetObjectItem(o, "parent");
            item->parent_id = par ? par->valueint : -1;
            cJSON *ti = cJSON_GetObjectItem(o, "tab_index");
            item->tab_index = ti ? ti->valueint : 0;
            cJSON *lk = cJSON_GetObjectItem(o, "locked");
            item->locked = lk ? lk->valueint : false;
            cJSON *vis = cJSON_GetObjectItem(o, "visible");
            item->visible = vis ? vis->valueint : true;
            if (item->id >= p->next_item_id) p->next_item_id = item->id + 1;
        }
    }

    cJSON *nodes_arr = cJSON_GetObjectItem(root, "nodes");
    if (nodes_arr && cJSON_IsArray(nodes_arr)) {
        p->node_count = cJSON_GetArraySize(nodes_arr);
        p->nodes = calloc(p->node_count, sizeof(GraphNode));
        for (int i = 0; i < p->node_count; i++) {
            cJSON *o = cJSON_GetArrayItem(nodes_arr, i);
            GraphNode *n = &p->nodes[i];
            n->id = cJSON_GetObjectItem(o, "id")->valueint;
            n->type = cJSON_GetObjectItem(o, "type")->valueint;
            cJSON *lbl = cJSON_GetObjectItem(o, "label");
            n->label = strdup(lbl && lbl->valuestring ? lbl->valuestring : "");
            cJSON *wid = cJSON_GetObjectItem(o, "widget_id");
            n->widget_id = wid ? wid->valueint : -1;
            cJSON *sk = cJSON_GetObjectItem(o, "store_key");
            if (sk && sk->valuestring) n->store_key = strdup(sk->valuestring);
            cJSON *fs = cJSON_GetObjectItem(o, "fil_script");
            if (fs && fs->valuestring) n->fil_script = strdup(fs->valuestring);
            cJSON *ports_arr = cJSON_GetObjectItem(o, "ports");
            if (ports_arr && cJSON_IsArray(ports_arr)) {
                n->port_count = cJSON_GetArraySize(ports_arr);
                n->ports = calloc(n->port_count, sizeof(PortDef));
                for (int j = 0; j < n->port_count; j++) {
                    cJSON *po = cJSON_GetArrayItem(ports_arr, j);
                    n->ports[j].id = cJSON_GetObjectItem(po, "id")->valueint;
                    cJSON *pn = cJSON_GetObjectItem(po, "name");
                    n->ports[j].name = strdup(pn && pn->valuestring ? pn->valuestring : "");
                    n->ports[j].type = cJSON_GetObjectItem(po, "type")->valueint;
                    n->ports[j].is_output = cJSON_GetObjectItem(po, "out")->valueint;
                }
            }
        }
    }

    cJSON *edges_arr = cJSON_GetObjectItem(root, "edges");
    if (edges_arr && cJSON_IsArray(edges_arr)) {
        p->edge_count = cJSON_GetArraySize(edges_arr);
        p->edges = calloc(p->edge_count, sizeof(ConnectionEdge));
        for (int i = 0; i < p->edge_count; i++) {
            cJSON *o = cJSON_GetArrayItem(edges_arr, i);
            p->edges[i].from_node = cJSON_GetObjectItem(o, "from_node")->valueint;
            p->edges[i].from_port = cJSON_GetObjectItem(o, "from_port")->valueint;
            p->edges[i].to_node = cJSON_GetObjectItem(o, "to_node")->valueint;
            p->edges[i].to_port = cJSON_GetObjectItem(o, "to_port")->valueint;
            cJSON *cond = cJSON_GetObjectItem(o, "condition");
            if (cond && cond->valuestring) p->edges[i].condition = strdup(cond->valuestring);
            cJSON *trans = cJSON_GetObjectItem(o, "transform");
            if (trans && trans->valuestring) p->edges[i].transform = strdup(trans->valuestring);
        }
    }

    cJSON *km_arr = cJSON_GetObjectItem(root, "keymaps");
    if (km_arr && cJSON_IsArray(km_arr)) {
        p->keymap_count = cJSON_GetArraySize(km_arr);
        p->keymaps = calloc(p->keymap_count, sizeof(KeymapEntry));
        for (int i = 0; i < p->keymap_count; i++) {
            cJSON *o = cJSON_GetArrayItem(km_arr, i);
            p->keymaps[i].scope = cJSON_GetObjectItem(o, "scope")->valueint;
            p->keymaps[i].widget_id = cJSON_GetObjectItem(o, "widget_id")->valueint;
            cJSON *k = cJSON_GetObjectItem(o, "key");
            p->keymaps[i].key = strdup(k && k->valuestring ? k->valuestring : "");
            cJSON *a = cJSON_GetObjectItem(o, "action");
            p->keymaps[i].action = strdup(a && a->valuestring ? a->valuestring : "");
        }
    }

    cJSON *sv_arr = cJSON_GetObjectItem(root, "store_vars");
    if (sv_arr && cJSON_IsArray(sv_arr)) {
        p->store_var_count = cJSON_GetArraySize(sv_arr);
        p->store_vars = calloc(p->store_var_count, sizeof(StoreVar));
        for (int i = 0; i < p->store_var_count; i++) {
            cJSON *o = cJSON_GetArrayItem(sv_arr, i);
            cJSON *k = cJSON_GetObjectItem(o, "key");
            p->store_vars[i].key = strdup(k && k->valuestring ? k->valuestring : "");
            cJSON *iv = cJSON_GetObjectItem(o, "initial");
            p->store_vars[i].initial_value = strdup(iv && iv->valuestring ? iv->valuestring : "");
            cJSON *t = cJSON_GetObjectItem(o, "type");
            p->store_vars[i].type = strdup(t && t->valuestring ? t->valuestring : "string");
        }
    }

    cJSON *tui = cJSON_GetObjectItem(root, "tui");
    if (tui) {
        p->tui.show_tui_overlay = cJSON_GetObjectItem(tui, "show_tab_order")->valueint;
        p->tui.min_terminal_width = cJSON_GetObjectItem(tui, "min_term_w")->valueint;
        p->tui.min_terminal_height = cJSON_GetObjectItem(tui, "min_term_h")->valueint;
        cJSON *ff = cJSON_GetObjectItem(tui, "fallback_font");
        if (ff && ff->valuestring) p->tui.fallback_font = strdup(ff->valuestring);
    }

    p->undo = undo_stack_new(200);
    cJSON_Delete(root);
    return p;
}

CanvasItem *project_add_item(BuilderProject *p, const char *widget_type, int x, int y, int w, int h) {
    p->item_count++;
    p->items = realloc(p->items, p->item_count * sizeof(CanvasItem));
    CanvasItem *item = &p->items[p->item_count - 1];
    memset(item, 0, sizeof(*item));
    item->id = p->next_item_id++;
    item->widget_type = strdup(widget_type);
    char buf[64];
    snprintf(buf, sizeof(buf), "%s_%d", widget_type, item->id);
    item->instance_name = strdup(buf);
    item->rect = rect_new(x, y, w, h);
    item->params = cJSON_CreateObject();
    cJSON_AddStringToObject(item->params, "title", widget_type);
    item->parent_id = -1;
    item->visible = true;
    return item;
}

void project_remove_item(BuilderProject *p, int item_id) {
    for (int i = 0; i < p->item_count; i++) {
        if (p->items[i].id == item_id) {
            free(p->items[i].widget_type);
            free(p->items[i].instance_name);
            if (p->items[i].params) cJSON_Delete(p->items[i].params);
            memmove(&p->items[i], &p->items[i + 1],
                    (p->item_count - i - 1) * sizeof(CanvasItem));
            p->item_count--;
            return;
        }
    }
}

CanvasItem *project_find_item(BuilderProject *p, int item_id) {
    for (int i = 0; i < p->item_count; i++)
        if (p->items[i].id == item_id) return &p->items[i];
    return NULL;
}

void project_move_item(BuilderProject *p, int item_id, int x, int y) {
    CanvasItem *item = project_find_item(p, item_id);
    if (item) { item->rect.x = x; item->rect.y = y; }
}

void project_resize_item(BuilderProject *p, int item_id, int w, int h) {
    CanvasItem *item = project_find_item(p, item_id);
    if (item) { item->rect.w = w; item->rect.h = h; }
}

void project_set_parent(BuilderProject *p, int item_id, int parent_id) {
    CanvasItem *item = project_find_item(p, item_id);
    if (item) item->parent_id = parent_id;
}

void project_update_item_params(BuilderProject *p, int item_id, cJSON *new_params) {
    CanvasItem *item = project_find_item(p, item_id);
    if (!item) return;
    if (item->params) cJSON_Delete(item->params);
    item->params = cJSON_Duplicate(new_params, 1);
}

GraphNode *project_add_node(BuilderProject *p, int widget_id) {
    CanvasItem *item = project_find_item(p, widget_id);
    if (!item) return NULL;
    p->node_count++;
    p->nodes = realloc(p->nodes, p->node_count * sizeof(GraphNode));
    GraphNode *n = &p->nodes[p->node_count - 1];
    memset(n, 0, sizeof(*n));
    n->id = p->node_count;
    n->type = NODE_WIDGET;
    n->label = strdup(item->instance_name);
    n->widget_id = widget_id;
    n->ports = NULL;
    n->port_count = project_get_widget_ports(item->widget_type, &n->ports);
    return n;
}

GraphNode *project_add_store_node(BuilderProject *p, const char *key, const char *initial, const char *type) {
    p->node_count++;
    p->nodes = realloc(p->nodes, p->node_count * sizeof(GraphNode));
    GraphNode *n = &p->nodes[p->node_count - 1];
    memset(n, 0, sizeof(*n));
    n->id = p->node_count;
    n->type = NODE_STORE;
    char buf[128];
    snprintf(buf, sizeof(buf), "store.%s", key);
    n->label = strdup(buf);
    n->store_key = strdup(key);
    n->ports = malloc(2 * sizeof(PortDef));
    n->ports[0] = (PortDef){0, "value", PORT_STRING, true, "Current value"};
    n->ports[1] = (PortDef){1, "set", PORT_STRING, false, "Set the value"};
    n->port_count = 2;
    project_add_store_var(p, key, initial, type);
    return n;
}

void project_remove_node(BuilderProject *p, int node_id) {
    for (int i = 0; i < p->node_count; i++) {
        if (p->nodes[i].id == node_id) {
            free(p->nodes[i].label);
            free(p->nodes[i].store_key);
            free(p->nodes[i].fil_script);
            free(p->nodes[i].ports);
            memmove(&p->nodes[i], &p->nodes[i + 1],
                    (p->node_count - i - 1) * sizeof(GraphNode));
            p->node_count--;
            return;
        }
    }
}

bool project_add_edge(BuilderProject *p, int from_node, int from_port, int to_node, int to_port) {
    p->edge_count++;
    p->edges = realloc(p->edges, p->edge_count * sizeof(ConnectionEdge));
    ConnectionEdge *e = &p->edges[p->edge_count - 1];
    memset(e, 0, sizeof(*e));
    e->from_node = from_node;
    e->from_port = from_port;
    e->to_node = to_node;
    e->to_port = to_port;
    return true;
}

void project_remove_edge(BuilderProject *p, int edge_idx) {
    if (edge_idx < 0 || edge_idx >= p->edge_count) return;
    free(p->edges[edge_idx].condition);
    free(p->edges[edge_idx].transform);
    memmove(&p->edges[edge_idx], &p->edges[edge_idx + 1],
            (p->edge_count - edge_idx - 1) * sizeof(ConnectionEdge));
    p->edge_count--;
}

void project_add_keymap(BuilderProject *p, KeymapScope scope, int widget_id, const char *key, const char *action) {
    p->keymap_count++;
    p->keymaps = realloc(p->keymaps, p->keymap_count * sizeof(KeymapEntry));
    p->keymaps[p->keymap_count - 1].scope = scope;
    p->keymaps[p->keymap_count - 1].widget_id = widget_id;
    p->keymaps[p->keymap_count - 1].key = strdup(key);
    p->keymaps[p->keymap_count - 1].action = strdup(action);
}

void project_remove_keymap(BuilderProject *p, int idx) {
    if (idx < 0 || idx >= p->keymap_count) return;
    free(p->keymaps[idx].key);
    free(p->keymaps[idx].action);
    memmove(&p->keymaps[idx], &p->keymaps[idx + 1],
            (p->keymap_count - idx - 1) * sizeof(KeymapEntry));
    p->keymap_count--;
}

void project_add_store_var(BuilderProject *p, const char *key, const char *initial, const char *type) {
    for (int i = 0; i < p->store_var_count; i++) {
        if (strcmp(p->store_vars[i].key, key) == 0) return;
    }
    p->store_var_count++;
    p->store_vars = realloc(p->store_vars, p->store_var_count * sizeof(StoreVar));
    p->store_vars[p->store_var_count - 1].key = strdup(key);
    p->store_vars[p->store_var_count - 1].initial_value = strdup(initial ? initial : "");
    p->store_vars[p->store_var_count - 1].type = strdup(type ? type : "string");
}