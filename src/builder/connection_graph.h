#pragma once
#include "project.h"
#include "core/render.h"
#include <stdbool.h>
#include <core/event.h>

typedef struct {
    BuilderProject *project;
    int *node_x;
    int *node_y;
    int node_count;
    int *port_x;
    int *port_y;
    int total_ports;
    int selected_node;
    int selected_edge;
    int hovered_port;
    int wire_start_node;
    int wire_start_port;
    int wire_end_x;
    int wire_end_y;
    bool dragging_node;
    int drag_node_id;
    int drag_start_x;
    int drag_start_y;
    int drag_orig_x;
    int drag_orig_y;
    int scroll_x;
    int scroll_y;
    float zoom;
    bool editing_edge;
    int editing_edge_idx;
    char edit_buf[256];
    int edit_len;
    int edit_field;
} ConnectionGraph;

ConnectionGraph *graph_new(BuilderProject *p);
void graph_free(ConnectionGraph *g);
void graph_layout(ConnectionGraph *g);
void graph_render(ConnectionGraph *g, RenderTree *out, Rect area);
bool graph_mouse_down(ConnectionGraph *g, int x, int y);
bool graph_mouse_move(ConnectionGraph *g, int x, int y);
bool graph_mouse_up(ConnectionGraph *g, int x, int y);
void graph_key(ConnectionGraph *g, KeyCode code, char ch);
void graph_sync_nodes(ConnectionGraph *g);
void graph_delete_selected(ConnectionGraph *g);
void graph_add_store_node(ConnectionGraph *g, const char *key);
void graph_add_custom_port(ConnectionGraph *g, int node_id, const char *name, PortDataType type, bool is_output);
int graph_find_port_at(ConnectionGraph *g, int x, int y);
int graph_find_node_at(ConnectionGraph *g, int x, int y);
int graph_find_edge_at(ConnectionGraph *g, int x, int y);
bool graph_ports_compatible(PortDef *src, PortDef *dst);