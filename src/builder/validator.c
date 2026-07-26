#include "validator.h"
#include "script/fil.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

static void add_issue(ValidationReport *r, ValidationSeverity sev, const char *msg,
                      int item_id, int node_id, int edge_idx, int keymap_idx) {
    r->count++;
    r->issues = realloc(r->issues, r->count * sizeof(ValidationIssue));
    r->issues[r->count - 1].severity = sev;
    r->issues[r->count - 1].message = strdup(msg);
    r->issues[r->count - 1].item_id = item_id;
    r->issues[r->count - 1].node_id = node_id;
    r->issues[r->count - 1].edge_idx = edge_idx;
    r->issues[r->count - 1].keymap_idx = keymap_idx;
}

static bool rects_overlap(Rect a, Rect b) {
    return !(a.x + a.w <= b.x || b.x + b.w <= a.x ||
             a.y + a.h <= b.y || b.y + b.h <= a.y);
}

__attribute__((unused)) static double relative_luminance(uint32_t c) {
    double r = ((c >> 16) & 0xFF) / 255.0;
    double g = ((c >> 8) & 0xFF) / 255.0;
    double b = (c & 0xFF) / 255.0;
    r = r <= 0.03928 ? r / 12.92 : pow((r + 0.055) / 1.055, 2.4);
    g = g <= 0.03928 ? g / 12.92 : pow((g + 0.055) / 1.055, 2.4);
    b = b <= 0.03928 ? b / 12.92 : pow((b + 0.055) / 1.055, 2.4);
    return 0.2126 * r + 0.7152 * g + 0.0722 * b;
}

ValidationReport *validator_check_all(BuilderProject *p) {
    ValidationReport *r = calloc(1, sizeof(ValidationReport));

    for (int i = 0; i < p->item_count; i++) {
        for (int j = i + 1; j < p->item_count; j++) {
            if (p->items[i].parent_id == p->items[j].parent_id &&
                rects_overlap(p->items[i].rect, p->items[j].rect)) {
                char buf[256];
                snprintf(buf, sizeof(buf), "Widgets '%s' and '%s' overlap",
                         p->items[i].instance_name, p->items[j].instance_name);
                add_issue(r, V_WARNING, buf, p->items[i].id, -1, -1, -1);
            }
        }
        if (p->items[i].id == p->items[i].parent_id) {
            char buf[256];
            snprintf(buf, sizeof(buf), "Widget '%s' is its own parent",
                     p->items[i].instance_name);
            add_issue(r, V_ERROR, buf, p->items[i].id, -1, -1, -1);
        }
        if (p->items[i].rect.w <= 0 || p->items[i].rect.h <= 0) {
            char buf[256];
            snprintf(buf, sizeof(buf), "Widget '%s' has invalid dimensions (%dx%d)",
                     p->items[i].instance_name, p->items[i].rect.w, p->items[i].rect.h);
            add_issue(r, V_ERROR, buf, p->items[i].id, -1, -1, -1);
        }
        if (p->items[i].rect.x + p->items[i].rect.w > p->root_width ||
            p->items[i].rect.y + p->items[i].rect.h > p->root_height) {
            char buf[256];
            snprintf(buf, sizeof(buf), "Widget '%s' extends beyond root bounds",
                     p->items[i].instance_name);
            add_issue(r, V_WARNING, buf, p->items[i].id, -1, -1, -1);
        }
        if (p->items[i].tab_index == 0) {
            char buf[256];
            snprintf(buf, sizeof(buf), "Widget '%s' has no tab index set",
                     p->items[i].instance_name);
            add_issue(r, V_INFO, buf, p->items[i].id, -1, -1, -1);
        }
    }

    for (int i = 0; i < p->item_count; i++) {
        for (int j = i + 1; j < p->item_count; j++) {
            if (p->items[i].tab_index > 0 &&
                p->items[i].tab_index == p->items[j].tab_index) {
                char buf[256];
                snprintf(buf, sizeof(buf), "Duplicate tab index %d for '%s' and '%s'",
                         p->items[i].tab_index,
                         p->items[i].instance_name, p->items[j].instance_name);
                add_issue(r, V_WARNING, buf, p->items[i].id, -1, -1, -1);
            }
        }
    }

    for (int i = 0; i < p->item_count; i++) {
        CanvasItem *item = &p->items[i];
        bool is_interactive = false;
        if (strcmp(item->widget_type, "input") == 0 ||
            strcmp(item->widget_type, "password") == 0 ||
            strcmp(item->widget_type, "menu") == 0 ||
            strcmp(item->widget_type, "checklist") == 0 ||
            strcmp(item->widget_type, "toggle") == 0 ||
            strcmp(item->widget_type, "checkbox") == 0 ||
            strcmp(item->widget_type, "button") == 0 ||
            strcmp(item->widget_type, "yesno") == 0) {
            is_interactive = true;
        }
        if (is_interactive && item->tab_index == 0) {
            char buf[256];
            snprintf(buf, sizeof(buf), "Interactive widget '%s' needs a tab index",
                     item->instance_name);
            add_issue(r, V_WARNING, buf, item->id, -1, -1, -1);
        }
    }

    for (int i = 0; i < p->node_count; i++) {
        bool has_output = false;
        for (int j = 0; j < p->nodes[i].port_count; j++) {
            if (p->nodes[i].ports[j].is_output) has_output = true;
        }
        if (has_output) {
            bool connected = false;
            for (int j = 0; j < p->edge_count; j++) {
                if (p->edges[j].from_node == i) { connected = true; break; }
            }
            if (!connected && p->nodes[i].type == NODE_WIDGET) {
                char buf[256];
                snprintf(buf, sizeof(buf), "Node '%s' has unconnected output ports",
                         p->nodes[i].label);
                add_issue(r, V_INFO, buf, -1, i, -1, -1);
            }
        }
    }

    for (int i = 0; i < p->edge_count; i++) {
        ConnectionEdge *e = &p->edges[i];
        bool has_cycle = false;
        if (e->from_node == e->to_node) has_cycle = true;
        for (int j = i + 1; j < p->edge_count && !has_cycle; j++) {
            if (p->edges[j].from_node == e->to_node &&
                p->edges[j].to_node == e->from_node) has_cycle = true;
        }
        if (has_cycle) {
            char buf[256];
            snprintf(buf, sizeof(buf), "Cycle detected involving edge %d", i);
            add_issue(r, V_ERROR, buf, -1, -1, i, -1);
        }
    }

    for (int i = 0; i < p->node_count; i++) {
        if (p->nodes[i].type == NODE_FIL_BLOCK && p->nodes[i].fil_script) {
            FilResult *fr = fil_eval(p->nodes[i].fil_script, NULL, NULL);
            if (fr && fr->error_msg) {
                char buf[512];
                snprintf(buf, sizeof(buf), "FIL error in node '%s': %s",
                         p->nodes[i].label, fr->error_msg);
                add_issue(r, V_ERROR, buf, -1, i, -1, -1);
            }
            if (fr) fil_result_free(fr);
        }
    }

    for (int i = 0; i < p->store_var_count; i++) {
        bool used = false;
        for (int j = 0; j < p->node_count; j++) {
            if (p->nodes[j].store_key &&
                strcmp(p->nodes[j].store_key, p->store_vars[i].key) == 0) {
                used = true;
                break;
            }
        }
        if (!used) {
            char buf[256];
            snprintf(buf, sizeof(buf), "Store variable '%s' is declared but never used",
                     p->store_vars[i].key);
            add_issue(r, V_INFO, buf, -1, -1, -1, -1);
        }
    }

    if (p->keymap_count == 0 && p->item_count > 0) {
        add_issue(r, V_WARNING, "No keybindings defined", -1, -1, -1, -1);
    }

    return r;
}

void validation_report_free(ValidationReport *r) {
    if (!r) return;
    for (int i = 0; i < r->count; i++) free(r->issues[i].message);
    free(r->issues);
    free(r);
}