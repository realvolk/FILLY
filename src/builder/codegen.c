#include "codegen.h"
#include "script/fil.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>

static void add_error(CodegenResult *r, const char *msg) {
    r->error_count++;
    r->errors = realloc(r->errors, r->error_count * sizeof(char *));
    r->errors[r->error_count - 1] = strdup(msg);
}

static void add_warning(CodegenResult *r, const char *msg) {
    r->warning_count++;
    r->warnings = realloc(r->warnings, r->warning_count * sizeof(char *));
    r->warnings[r->warning_count - 1] = strdup(msg);
}

void codegen_result_free(CodegenResult *r) {
    if (!r) return;
    for (int i = 0; i < r->error_count; i++) free(r->errors[i]);
    free(r->errors);
    for (int i = 0; i < r->warning_count; i++) free(r->warnings[i]);
    free(r->warnings);
    memset(r, 0, sizeof(*r));
}

char *codegen_fil_script(BuilderProject *p) {
    if (!p) return strdup("");
    int cap = 4096;
    int len = 0;
    char *out = malloc(cap);
    out[0] = '\0';

    for (int i = 0; i < p->edge_count; i++) {
        ConnectionEdge *e = &p->edges[i];
        if (e->from_node < 0 || e->from_node >= p->node_count) continue;
        if (e->to_node < 0 || e->to_node >= p->node_count) continue;

        GraphNode *src = &p->nodes[e->from_node];
        GraphNode *dst = &p->nodes[e->to_node];

        if (e->from_port < 0 || e->from_port >= src->port_count) continue;
        if (e->to_port < 0 || e->to_port >= dst->port_count) continue;

        PortDef *src_port = &src->ports[e->from_port];
        PortDef *dst_port = &dst->ports[e->to_port];

        if (!src_port->is_output) continue;
        if (dst_port->is_output) continue;

        char block[1024];
        int n = 0;

        if (src->type == NODE_WIDGET && src_port->type == PORT_TRIGGER) {
            if (dst->type == NODE_STORE && strcmp(dst_port->name, "set") == 0) {
                n = snprintf(block, sizeof(block),
                    "when widget.%s.%s then\n"
                    "  set store.%s to value of %s\n"
                    "end\n\n",
                    src->label, src_port->name,
                    dst->store_key ? dst->store_key : "unknown",
                    src->label);
            } else if (dst->type == NODE_WIDGET && strcmp(dst_port->name, "set_visible") == 0) {
                n = snprintf(block, sizeof(block),
                    "when widget.%s.%s then\n"
                    "  show widget.%s\n"
                    "end\n\n",
                    src->label, src_port->name, dst->label);
            } else if (dst->type == NODE_WIDGET && strcmp(dst_port->name, "play_animation") == 0) {
                n = snprintf(block, sizeof(block),
                    "when widget.%s.%s then\n"
                    "  animate \"%s\" with \"fadeIn\"\n"
                    "end\n\n",
                    src->label, src_port->name, dst->label);
            } else {
                n = snprintf(block, sizeof(block),
                    "when widget.%s.%s then\n"
                    "  -- connected to %s.%s\n"
                    "end\n\n",
                    src->label, src_port->name, dst->label, dst_port->name);
            }
        } else if (src->type == NODE_STORE && strcmp(src_port->name, "value") == 0) {
            if (dst->type == NODE_WIDGET && strcmp(dst_port->name, "set_value") == 0) {
                n = snprintf(block, sizeof(block),
                    "when store.%s changes then\n"
                    "  set value of %s to store.%s\n"
                    "end\n\n",
                    src->store_key ? src->store_key : "unknown",
                    dst->label,
                    src->store_key ? src->store_key : "unknown");
            } else {
                n = snprintf(block, sizeof(block),
                    "when store.%s changes then\n"
                    "  -- connected to %s.%s\n"
                    "end\n\n",
                    src->store_key ? src->store_key : "unknown",
                    dst->label, dst_port->name);
            }
        } else if (src->type == NODE_WIDGET && strcmp(src_port->name, "animation_end") == 0) {
            if (dst->type == NODE_STORE && strcmp(dst_port->name, "set") == 0) {
                n = snprintf(block, sizeof(block),
                    "when widget.%s.animation_end then\n"
                    "  set store.%s to \"1\"\n"
                    "end\n\n",
                    src->label,
                    dst->store_key ? dst->store_key : "unknown");
            } else {
                n = snprintf(block, sizeof(block),
                    "when widget.%s.animation_end then\n"
                    "  -- connected to %s.%s\n"
                    "end\n\n",
                    src->label, dst->label, dst_port->name);
            }
        } else {
            n = snprintf(block, sizeof(block),
                "-- Edge: %s.%s -> %s.%s\n\n",
                src->label, src_port->name, dst->label, dst_port->name);
        }

        if (n > 0 && n < (int)sizeof(block)) {
            if (len + n + 1 > cap) {
                cap = cap * 2 + n;
                out = realloc(out, cap);
            }
            memcpy(out + len, block, n);
            len += n;
            out[len] = '\0';
        }
    }

    for (int i = 0; i < p->node_count; i++) {
        if (p->nodes[i].type == NODE_FIL_BLOCK && p->nodes[i].fil_script) {
            int n = strlen(p->nodes[i].fil_script);
            if (len + n + 3 > cap) {
                cap = cap * 2 + n;
                out = realloc(out, cap);
            }
            memcpy(out + len, p->nodes[i].fil_script, n);
            len += n;
            out[len] = '\0';
            if (out[len - 1] != '\n') {
                out[len++] = '\n';
                out[len] = '\0';
            }
            out[len++] = '\n';
            out[len] = '\0';
        }
    }

    return out;
}

CodegenResult codegen_c_plugin(BuilderProject *p, const char *output_dir) {
    CodegenResult result = {0};
    if (!p || !output_dir) {
        add_error(&result, "Invalid project or output directory");
        return result;
    }

    mkdir(output_dir, 0755);

    char path_c[1024];
    snprintf(path_c, sizeof(path_c), "%s/%s.c", output_dir, p->project_name);
    FILE *f = fopen(path_c, "w");
    if (!f) {
        add_error(&result, "Cannot create output C file");
        return result;
    }

    fprintf(f, "#include \"core/widget.h\"\n");
    fprintf(f, "#include \"core/widget_base.h\"\n");
    fprintf(f, "#include \"core/render.h\"\n");
    fprintf(f, "#include \"core/session.h\"\n");
    fprintf(f, "#include \"core/store.h\"\n");
    fprintf(f, "#include \"core/arena.h\"\n");
    fprintf(f, "#include \"core/theme.h\"\n");
    fprintf(f, "#include \"core/animation.h\"\n");
    fprintf(f, "#include \"script/fil.h\"\n");
    fprintf(f, "#include <stdlib.h>\n");
    fprintf(f, "#include <string.h>\n\n");

    char *fil = codegen_fil_script(p);
    fprintf(f, "static const char *fil_script =\n");
    fprintf(f, "    \"%s\";\n\n", fil);
    free(fil);

    fprintf(f, "typedef struct {\n");
    fprintf(f, "    WidgetBase base;\n");
    fprintf(f, "    RenderTree *layout;\n");
    fprintf(f, "    Arena *arena;\n");
    fprintf(f, "    Theme *theme;\n");
    fprintf(f, "    Store *store;\n");
    fprintf(f, "} %sData;\n\n", p->project_name);

    fprintf(f, "static RenderTree *build_layout(Arena *arena, Theme *theme) {\n");
    fprintf(f, "    RenderTree *root = arena_alloc(arena, sizeof(RenderTree));\n");
    fprintf(f, "    memset(root, 0, sizeof(*root));\n");
    fprintf(f, "    root->type = RNODE_CONTAINER;\n");
    fprintf(f, "    root->rect = rect_new(0, 0, %d, %d);\n", p->root_width, p->root_height);
    fprintf(f, "    root->resolved_style = widgetstyle_default();\n");
    fprintf(f, "    root->container.padding = edgeinsets_zero();\n\n");

    int top_level = 0;
    for (int i = 0; i < p->item_count; i++)
        if (p->items[i].parent_id == -1) top_level++;

    fprintf(f, "    RenderTree *children = arena_alloc(arena, %d * sizeof(RenderTree));\n", top_level);
    int idx = 0;
    for (int i = 0; i < p->item_count; i++) {
        CanvasItem *item = &p->items[i];
        if (item->parent_id != -1) continue;
        fprintf(f, "    children[%d].type = RNODE_TEXT;\n", idx);
        fprintf(f, "    children[%d].rect = rect_new(%d, %d, %d, %d);\n",
                idx, item->rect.x, item->rect.y, item->rect.w, item->rect.h);
        fprintf(f, "    children[%d].resolved_style = widgetstyle_default();\n", idx);
        fprintf(f, "    children[%d].resolved_style.font_size = 14;\n", idx);
        fprintf(f, "    children[%d].text.content = arena_strdup(arena, \"%s\");\n",
                idx, item->instance_name);
        idx++;
    }

    fprintf(f, "    root->container.children = children;\n");
    fprintf(f, "    root->container.child_count = %d;\n", top_level);
    fprintf(f, "    return root;\n");
    fprintf(f, "}\n\n");

    fprintf(f, "static void %s_render(Widget *self, Rect area, RenderTree *out) {\n", p->project_name);
    fprintf(f, "    %sData *d = (%sData *)(self + 1);\n", p->project_name, p->project_name);
    fprintf(f, "    arena_reset(d->arena);\n");
    fprintf(f, "    RenderTree *layout = build_layout(d->arena, d->theme);\n");
    fprintf(f, "    resolve_node_styles(layout, d->theme);\n");
    fprintf(f, "    *out = *layout;\n");
    fprintf(f, "}\n\n");

    fprintf(f, "static EventResult %s_handle_event(Widget *self, Event *ev, Backend *backend) {\n", p->project_name);
    fprintf(f, "    %sData *d = (%sData *)(self + 1);\n", p->project_name, p->project_name);
    fprintf(f, "    if (ev->type == EVENT_KEY && ev->code == KEY_ESC)\n");
    fprintf(f, "        return event_result_response((WidgetResponse){NULL, true});\n");
    fprintf(f, "    FilResult *fr = fil_eval(fil_script, store_get_safe, NULL);\n");
    fprintf(f, "    if (fr && !fr->accepted) {\n");
    fprintf(f, "        EventResult er = event_result_response((WidgetResponse){NULL, true, fr->error_msg});\n");
    fprintf(f, "        fil_result_free(fr);\n");
    fprintf(f, "        return er;\n");
    fprintf(f, "    }\n");
    fprintf(f, "    if (fr) fil_result_free(fr);\n");
    fprintf(f, "    return event_result_unhandled();\n");
    fprintf(f, "}\n\n");

    fprintf(f, "static void %s_destroy(Widget *self) {\n", p->project_name);
    fprintf(f, "    %sData *d = (%sData *)(self + 1);\n", p->project_name, p->project_name);
    fprintf(f, "    if (d->arena) arena_free(d->arena);\n");
    fprintf(f, "    if (d->theme) theme_free(d->theme);\n");
    fprintf(f, "    if (d->store) store_free(d->store);\n");
    fprintf(f, "}\n\n");

    fprintf(f, "Widget *%s_new(void) {\n", p->project_name);
    fprintf(f, "    Widget *w = calloc(1, sizeof(Widget) + sizeof(%sData));\n", p->project_name);
    fprintf(f, "    %sData *d = (%sData *)(w + 1);\n", p->project_name, p->project_name);
    fprintf(f, "    d->base.dirty = true;\n");
    fprintf(f, "    d->arena = arena_new(1024 * 1024);\n");
    fprintf(f, "    d->theme = theme_default();\n");
    fprintf(f, "    d->store = store_new();\n");
    fprintf(f, "    widget_base_init(w, d, sizeof(%sData), %s_render, %s_handle_event, %s_destroy);\n",
            p->project_name, p->project_name, p->project_name, p->project_name);
    fprintf(f, "    return w;\n");
    fprintf(f, "}\n\n");

    fprintf(f, "void register_plugins(void (*reg)(const char *, WidgetFactory)) {\n");
    fprintf(f, "    reg(\"%s\", %s_new);\n", p->project_name, p->project_name);
    fprintf(f, "}\n");

    fclose(f);

    char path_make[1024];
    snprintf(path_make, sizeof(path_make), "%s/Makefile", output_dir);
    FILE *mf = fopen(path_make, "w");
    if (mf) {
        fprintf(mf, "CC = gcc\n");
        fprintf(mf, "CFLAGS = -std=c99 -Wall -O2 -fPIC -I$(FILLY_DIR)/src\n");
        fprintf(mf, "LDFLAGS = -shared -lsodium -lm\n");
        fprintf(mf, "OBJ = %s.o\n\n", p->project_name);
        fprintf(mf, "%s.so: $(OBJ)\n", p->project_name);
        fprintf(mf, "\t$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^\n\n");
        fprintf(mf, "%%.o: %%.c\n");
        fprintf(mf, "\t$(CC) $(CFLAGS) -c $<\n\n");
        fprintf(mf, "clean:\n");
        fprintf(mf, "\trm -f *.o *.so\n");
        fclose(mf);
    }

    return result;
}

CodegenResult codegen_validate(BuilderProject *p) {
    CodegenResult result = {0};
    if (!p) {
        add_error(&result, "Project is NULL");
        return result;
    }
    if (p->item_count == 0)
        add_warning(&result, "Project has no widgets");
    char *fil = codegen_fil_script(p);
    if (fil && strlen(fil) > 0) {
        FilResult *fr = fil_eval(fil, NULL, NULL);
        if (fr && fr->error_msg)
            add_error(&result, fr->error_msg);
        if (fr) fil_result_free(fr);
    }
    free(fil);
    return result;
}