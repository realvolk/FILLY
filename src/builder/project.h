#pragma once
#include "core/render.h"
#include "cJSON.h"
#include "core/undo.h"
#include "protocol/protocol.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum { PORT_TRIGGER, PORT_STRING, PORT_BOOL, PORT_INT } PortDataType;

typedef struct {
    int id;
    char *name;
    PortDataType type;
    bool is_output;
    char *description;
} PortDef;

typedef struct {
    int from_node;
    int from_port;
    int to_node;
    int to_port;
    char *condition;
    char *transform;
} ConnectionEdge;

typedef enum { NODE_WIDGET, NODE_STORE, NODE_FIL_BLOCK, NODE_CONDITIONAL } NodeType;

typedef struct GraphNode {
    int id;
    NodeType type;
    char *label;
    int widget_id;
    char *store_key;
    char *fil_script;
    PortDef *ports;
    int port_count;
} GraphNode;

typedef struct {
    int id;
    char *widget_type;
    char *instance_name;
    Rect rect;
    cJSON *params;
    int parent_id;
    int tab_index;
    bool locked;
    bool visible;
} CanvasItem;

typedef enum { KB_SCOPE_GLOBAL, KB_SCOPE_WIDGET } KeymapScope;

typedef struct {
    KeymapScope scope;
    int widget_id;
    char *key;
    char *action;
} KeymapEntry;

typedef struct {
    char *key;
    char *initial_value;
    char *type;
} StoreVar;

typedef struct {
    bool show_tui_overlay;
    int min_terminal_width;
    int min_terminal_height;
    char *fallback_font;
} TuiConfig;

typedef struct {
    char *project_name;
    char *file_path;
    CanvasItem *items;
    int item_count;
    int next_item_id;
    int root_width;
    int root_height;
    GraphNode *nodes;
    int node_count;
    ConnectionEdge *edges;
    int edge_count;
    KeymapEntry *keymaps;
    int keymap_count;
    int store_var_count;
    StoreVar *store_vars;
    TuiConfig tui;
    int filly_version;
    char *author;
    char *description;
    UndoStack *undo;
} BuilderProject;

BuilderProject *project_new(const char *name, int root_w, int root_h);
void project_free(BuilderProject *p);
bool project_save(BuilderProject *p, const char *path);
BuilderProject *project_load(const char *path);

CanvasItem *project_add_item(BuilderProject *p, const char *widget_type, int x, int y, int w, int h);
void project_remove_item(BuilderProject *p, int item_id);
CanvasItem *project_find_item(BuilderProject *p, int item_id);
void project_move_item(BuilderProject *p, int item_id, int x, int y);
void project_resize_item(BuilderProject *p, int item_id, int w, int h);
void project_set_parent(BuilderProject *p, int item_id, int parent_id);

GraphNode *project_add_node(BuilderProject *p, int widget_id);
GraphNode *project_add_store_node(BuilderProject *p, const char *key, const char *initial, const char *type);
void project_remove_node(BuilderProject *p, int node_id);
bool project_add_edge(BuilderProject *p, int from_node, int from_port, int to_node, int to_port);
void project_remove_edge(BuilderProject *p, int edge_idx);

void project_add_keymap(BuilderProject *p, KeymapScope scope, int widget_id, const char *key, const char *action);
void project_remove_keymap(BuilderProject *p, int idx);

void project_add_store_var(BuilderProject *p, const char *key, const char *initial, const char *type);

int project_get_widget_ports(const char *widget_type, PortDef **ports);

void project_update_item_params(BuilderProject *p, int item_id, cJSON *new_params);