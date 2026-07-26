#include "connection_graph.h"
#include "core/session.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <core/event.h>

extern Arena *g_session_arena;

#define NODE_WIDTH 200
#define NODE_PORT_HEIGHT 18
#define PORT_RADIUS 6
#define HEADER_HEIGHT 22

__attribute__((unused)) static int count_outputs(GraphNode *n) {
    int c = 0;
    for (int i = 0; i < n->port_count; i++)
        if (n->ports[i].is_output) c++;
    return c;
}

__attribute__((unused)) static int count_inputs(GraphNode *n) {
    int c = 0;
    for (int i = 0; i < n->port_count; i++)
        if (!n->ports[i].is_output) c++;
    return c;
}

ConnectionGraph *graph_new(BuilderProject *p) {
    ConnectionGraph *g = calloc(1, sizeof(ConnectionGraph));
    g->project = p;
    g->zoom = 1.0f;
    g->selected_node = -1;
    g->selected_edge = -1;
    g->hovered_port = -1;
    g->wire_start_node = -1;
    g->wire_start_port = -1;
    g->editing_edge = false;
    g->editing_edge_idx = -1;
    graph_sync_nodes(g);
    graph_layout(g);
    return g;
}

void graph_free(ConnectionGraph *g) {
    if (!g) return;
    free(g->node_x);
    free(g->node_y);
    free(g->port_x);
    free(g->port_y);
    free(g);
}

void graph_sync_nodes(ConnectionGraph *g) {
    free(g->node_x);
    free(g->node_y);
    free(g->port_x);
    free(g->port_y);
    g->node_count = g->project->node_count;
    g->node_x = calloc(g->node_count, sizeof(int));
    g->node_y = calloc(g->node_count, sizeof(int));
    g->total_ports = 0;
    for (int i = 0; i < g->node_count; i++)
        g->total_ports += g->project->nodes[i].port_count;
    g->port_x = calloc(g->total_ports, sizeof(int));
    g->port_y = calloc(g->total_ports, sizeof(int));
}

void graph_layout(ConnectionGraph *g) {
    if (g->node_count == 0) return;
    int cols = (int)sqrt(g->node_count) + 1;
    int spacing_x = NODE_WIDTH + 60;
    int spacing_y = 120;
    for (int i = 0; i < g->node_count; i++) {
        g->node_x[i] = 20 + (i % cols) * spacing_x;
        g->node_y[i] = 20 + (i / cols) * spacing_y;
    }
}

int graph_find_node_at(ConnectionGraph *g, int x, int y) {
    for (int i = g->node_count - 1; i >= 0; i--) {
        int nx = (int)((g->node_x[i] - g->scroll_x) * g->zoom);
        int ny = (int)((g->node_y[i] - g->scroll_y) * g->zoom);
        int nw = (int)(NODE_WIDTH * g->zoom);
        int nh = (int)((HEADER_HEIGHT + g->project->nodes[i].port_count * NODE_PORT_HEIGHT) * g->zoom);
        if (x >= nx && x < nx + nw && y >= ny && y < ny + nh)
            return i;
    }
    return -1;
}

int graph_find_port_at(ConnectionGraph *g, int x, int y) {
    int port_idx = 0;
    for (int ni = 0; ni < g->node_count; ni++) {
        GraphNode *node = &g->project->nodes[ni];
        int nx = (int)((g->node_x[ni] - g->scroll_x) * g->zoom);
        int ny = (int)((g->node_y[ni] - g->scroll_y) * g->zoom);
        int out_idx = 0;
        int in_idx = 0;
        for (int pi = 0; pi < node->port_count; pi++) {
            int px, py;
            if (node->ports[pi].is_output) {
                px = nx + (int)(NODE_WIDTH * g->zoom) + (int)(PORT_RADIUS * g->zoom);
                py = ny + (int)(HEADER_HEIGHT * g->zoom) + (int)(10 * g->zoom) + out_idx * (int)(NODE_PORT_HEIGHT * g->zoom);
                out_idx++;
            } else {
                px = nx - (int)(PORT_RADIUS * g->zoom);
                py = ny + (int)(HEADER_HEIGHT * g->zoom) + (int)(10 * g->zoom) + in_idx * (int)(NODE_PORT_HEIGHT * g->zoom);
                in_idx++;
            }
            if (port_idx < g->total_ports) {
                g->port_x[port_idx] = px;
                g->port_y[port_idx] = py;
            }
            int dx = x - px;
            int dy = y - py;
            if (dx * dx + dy * dy <= (int)(PORT_RADIUS * g->zoom * PORT_RADIUS * g->zoom * 2))
                return port_idx;
            port_idx++;
        }
    }
    return -1;
}

int graph_find_edge_at(ConnectionGraph *g, int x, int y) {
    for (int i = 0; i < g->project->edge_count; i++) {
        ConnectionEdge *e = &g->project->edges[i];
        int from_pidx = 0;
        for (int ni = 0; ni < e->from_node && ni < g->node_count; ni++)
            from_pidx += g->project->nodes[ni].port_count;
        from_pidx += e->from_port;
        int to_pidx = 0;
        for (int ni = 0; ni < e->to_node && ni < g->node_count; ni++)
            to_pidx += g->project->nodes[ni].port_count;
        to_pidx += e->to_port;
        if (from_pidx < g->total_ports && to_pidx < g->total_ports) {
            int fx = g->port_x[from_pidx];
            int fy = g->port_y[from_pidx];
            int tx = g->port_x[to_pidx];
            int ty = g->port_y[to_pidx];
            float dist = 1e9f;
            float cpx = fx + (tx - fx) * 0.5f;
            for (float s = 0.0f; s <= 1.0f; s += 0.05f) {
                float u = 1.0f - s;
                float bx = u * u * fx + 2 * u * s * cpx + s * s * tx;
                float by = u * u * fy + 2 * u * s * cpx + s * s * ty;
                float dx = x - bx;
                float dy = y - by;
                float d = dx * dx + dy * dy;
                if (d < dist) dist = d;
            }
            if (dist < 100.0f) return i;
        }
    }
    return -1;
}

bool graph_ports_compatible(PortDef *src, PortDef *dst) {
    if (!src->is_output) return false;
    if (dst->is_output) return false;
    if (src->type == PORT_TRIGGER) return true;
    if (src->type == dst->type) return true;
    if ((src->type == PORT_BOOL || src->type == PORT_INT) && dst->type == PORT_STRING) return true;
    return false;
}

bool graph_mouse_down(ConnectionGraph *g, int x, int y) {
    if (!g) return false;

    if (g->editing_edge) {
        g->editing_edge = false;
        g->editing_edge_idx = -1;
        return true;
    }

    g->hovered_port = graph_find_port_at(g, x, y);
    if (g->hovered_port >= 0) {
        int port_idx = 0;
        for (int ni = 0; ni < g->node_count; ni++) {
            GraphNode *node = &g->project->nodes[ni];
            for (int pi = 0; pi < node->port_count; pi++) {
                if (port_idx == g->hovered_port && node->ports[pi].is_output) {
                    g->wire_start_node = ni;
                    g->wire_start_port = pi;
                    g->wire_end_x = x;
                    g->wire_end_y = y;
                    return true;
                }
                port_idx++;
            }
        }
        g->hovered_port = -1;
        return false;
    }

    g->selected_edge = graph_find_edge_at(g, x, y);
    if (g->selected_edge >= 0) {
        g->selected_node = -1;
        g->editing_edge = true;
        g->editing_edge_idx = g->selected_edge;
        g->edit_len = 0;
        g->edit_buf[0] = '\0';
        g->edit_field = 0;
        return true;
    }

    g->selected_node = graph_find_node_at(g, x, y);
    if (g->selected_node >= 0) {
        g->dragging_node = true;
        g->drag_node_id = g->selected_node;
        g->drag_start_x = x;
        g->drag_start_y = y;
        g->drag_orig_x = g->node_x[g->selected_node];
        g->drag_orig_y = g->node_y[g->selected_node];
        g->selected_edge = -1;
        return true;
    }

    g->selected_node = -1;
    g->selected_edge = -1;
    return false;
}

bool graph_mouse_move(ConnectionGraph *g, int x, int y) {
    if (!g) return false;
    if (g->wire_start_node >= 0) {
        g->wire_end_x = x;
        g->wire_end_y = y;
        return true;
    }
    if (g->dragging_node && g->drag_node_id >= 0) {
        g->node_x[g->drag_node_id] = g->drag_orig_x + (x - g->drag_start_x);
        g->node_y[g->drag_node_id] = g->drag_orig_y + (y - g->drag_start_y);
        return true;
    }
    g->hovered_port = graph_find_port_at(g, x, y);
    return g->hovered_port >= 0;
}

bool graph_mouse_up(ConnectionGraph *g, int x, int y) {
    if (!g) return false;
    if (g->wire_start_node >= 0) {
        int target_port = graph_find_port_at(g, x, y);
        if (target_port >= 0) {
            int port_idx = 0;
            for (int ni = 0; ni < g->node_count; ni++) {
                GraphNode *node = &g->project->nodes[ni];
                for (int pi = 0; pi < node->port_count; pi++) {
                    if (port_idx == target_port && !node->ports[pi].is_output) {
                        GraphNode *src_node = &g->project->nodes[g->wire_start_node];
                        PortDef *src_port = &src_node->ports[g->wire_start_port];
                        PortDef *dst_port = &node->ports[pi];
                        if (graph_ports_compatible(src_port, dst_port)) {
                            project_add_edge(g->project, g->wire_start_node,
                                            g->wire_start_port, ni, pi);
                        }
                        break;
                    }
                    port_idx++;
                }
            }
        }
        g->wire_start_node = -1;
        g->wire_start_port = -1;
        return true;
    }
    g->dragging_node = false;
    return true;
}

void graph_key(ConnectionGraph *g, KeyCode code, char ch) {
    if (!g) return;

    if (g->editing_edge && g->editing_edge_idx >= 0) {
        if (code == KEY_ESC) {
            g->editing_edge = false;
            g->editing_edge_idx = -1;
            return;
        }
        if (code == KEY_ENTER) {
            ConnectionEdge *e = &g->project->edges[g->editing_edge_idx];
            if (g->edit_field == 0) {
                free(e->condition);
                e->condition = g->edit_len > 0 ? strdup(g->edit_buf) : NULL;
                g->edit_len = 0;
                g->edit_buf[0] = '\0';
                g->edit_field = 1;
            } else {
                free(e->transform);
                e->transform = g->edit_len > 0 ? strdup(g->edit_buf) : NULL;
                g->editing_edge = false;
                g->editing_edge_idx = -1;
            }
            return;
        }
        if (code == KEY_BACKSPACE && g->edit_len > 0) {
            g->edit_buf[--g->edit_len] = '\0';
            return;
        }
        if (code == KEY_CHAR && g->edit_len < 254 && ch >= 32) {
            g->edit_buf[g->edit_len++] = ch;
            g->edit_buf[g->edit_len] = '\0';
            return;
        }
        return;
    }

    if (code == KEY_DELETE) {
        graph_delete_selected(g);
    }
}

void graph_delete_selected(ConnectionGraph *g) {
    if (g->selected_edge >= 0) {
        project_remove_edge(g->project, g->selected_edge);
        g->selected_edge = -1;
    } else if (g->selected_node >= 0) {
        int node_id = g->project->nodes[g->selected_node].id;
        project_remove_node(g->project, node_id);
        g->selected_node = -1;
        graph_sync_nodes(g);
        graph_layout(g);
    }
}

void graph_add_store_node(ConnectionGraph *g, const char *key) {
    if (!g || !key) return;
    project_add_store_node(g->project, key, "", "string");
    graph_sync_nodes(g);
    graph_layout(g);
}

void graph_add_custom_port(ConnectionGraph *g, int node_id, const char *name, PortDataType type, bool is_output) {
    if (!g || !name || node_id < 0) return;
    for (int i = 0; i < g->node_count; i++) {
        if (g->project->nodes[i].id == node_id) {
            GraphNode *n = &g->project->nodes[i];
            n->port_count++;
            n->ports = realloc(n->ports, n->port_count * sizeof(PortDef));
            n->ports[n->port_count - 1].id = n->port_count;
            n->ports[n->port_count - 1].name = strdup(name);
            n->ports[n->port_count - 1].type = type;
            n->ports[n->port_count - 1].is_output = is_output;
            n->ports[n->port_count - 1].description = strdup("User-defined port");
            graph_sync_nodes(g);
            return;
        }
    }
}

void graph_render(ConnectionGraph *g, RenderTree *out, Rect area) {
    memset(out, 0, sizeof(*out));
    out->type = RNODE_CONTAINER;
    out->rect = area;
    out->container.border = BORDER_SINGLE;
    out->container.padding = edgeinsets_zero();

    int child_count = g->node_count + 1;
    RenderTree *children = arena_alloc(g_session_arena, child_count * sizeof(RenderTree));
    int idx = 0;

    if (g->wire_start_node >= 0) {
        RenderTree *wire = &children[idx++];
        memset(wire, 0, sizeof(*wire));
        wire->type = RNODE_TEXT;
        wire->rect = rect_new(0, 0, area.w - 2, 1);
        char buf[64];
        snprintf(buf, sizeof(buf), "Drawing wire from node %d port %d... (Esc to cancel)",
                 g->wire_start_node, g->wire_start_port);
        wire->text.content = arena_strdup(g_session_arena, buf);
        wire->style_class = "accent";
    } else if (g->editing_edge && g->editing_edge_idx >= 0) {
        RenderTree *edit = &children[idx++];
        memset(edit, 0, sizeof(*edit));
        edit->type = RNODE_TEXT;
        edit->rect = rect_new(0, 0, area.w - 2, 2);
        const char *field_name = g->edit_field == 0 ? "Condition" : "Transform";
        char buf[512];
        snprintf(buf, sizeof(buf), "Editing %s: %s_\n(Enter to confirm, Esc to cancel)",
                 field_name, g->edit_buf);
        edit->text.content = arena_strdup(g_session_arena, buf);
        edit->style_class = "accent";
    }

    for (int i = 0; i < g->node_count; i++) {
        GraphNode *node = &g->project->nodes[i];
        RenderTree *node_tree = &children[idx++];
        memset(node_tree, 0, sizeof(*node_tree));
        node_tree->type = RNODE_TEXT;
        int nx = (int)((g->node_x[i] - g->scroll_x) * g->zoom);
        int ny = (int)((g->node_y[i] - g->scroll_y) * g->zoom);
        int nw = (int)(NODE_WIDTH * g->zoom);
        int nh = (int)((HEADER_HEIGHT + node->port_count * NODE_PORT_HEIGHT) * g->zoom);
        node_tree->rect = rect_new(nx, ny, nw, nh);

        const char *type_str = node->type == NODE_WIDGET ? "Widget" :
                               node->type == NODE_STORE ? "Store" :
                               node->type == NODE_FIL_BLOCK ? "FIL" : "Cond";
        char buf[512];
        int off = snprintf(buf, sizeof(buf), "[%s] %s\n", type_str, node->label);
        for (int pi = 0; pi < node->port_count; pi++) {
            const char *dir = node->ports[pi].is_output ? "out" : "in ";
            const char *tname = node->ports[pi].type == PORT_TRIGGER ? "trig" :
                                node->ports[pi].type == PORT_STRING ? "str" :
                                node->ports[pi].type == PORT_BOOL ? "bool" : "int";
            off += snprintf(buf + off, sizeof(buf) - off, "  %s %s: %s\n",
                           dir, tname, node->ports[pi].name);
        }
        node_tree->text.content = arena_strdup(g_session_arena, buf);
        node_tree->style_class = (i == g->selected_node) ? "selected" : "normal";
        node_tree->resolved_style.font_size = 10;
    }

    out->container.children = children;
    out->container.child_count = idx;
}