#include "project.h"
#include "canvas.h"
#include "connection_graph.h"
#include "property_editor.h"
#include "codegen.h"
#include "validator.h"
#include "core/session.h"
#include "core/widget.h"
#include "core/widget_base.h"
#include "core/theme.h"
#include "core/arena.h"
#include "backend/headless/headless.h"
#include "backend/gcore/renderer.h"
#ifdef FILLY_GCORE
#include "backend/gcore/backend.h"
#endif
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

extern Theme *g_active_theme;
extern Arena *g_session_arena;
extern void register_builtin_widgets(void);

typedef enum {
    MODE_EDIT,
    MODE_WIRE,
    MODE_PREVIEW,
    MODE_TUI_PREVIEW
} BuilderMode;

typedef struct {
    WidgetBase base;
    BuilderProject *project;
    Canvas *canvas;
    ConnectionGraph *graph;
    PropertyEditor *prop_editor;
    BuilderMode mode;
    int active_pane;
    int palette_idx;
    char **palette_types;
    int palette_count;
    bool project_dirty;
    char *status_message;
    char *file_path;
    ValidationReport *last_report;
    bool show_validation;
    int validation_scroll;
    bool keymap_editing;
    int keymap_edit_idx;
    char keymap_buf[64];
    int keymap_buf_len;
} BuilderApp;

static const char *widget_categories[][20] = {
    {"input", "password", "form", "text_editor", "file_picker", NULL},
    {"menu", "checklist", "multiselect", "filter", "radio_group", "calendar", "color_picker", "range_slider", NULL},
    {"msg", "summary", "notification", "badge", "tooltip", "rich_text", "spinner", "separator", "gauge", NULL},
    {"toggle", "checkbox", "yesno", "button", NULL},
    {"tabs", "split_panes", "hub", NULL},
    {"progress", "disk", "table", "tree", "context_menu", "terminal_emulator", "widget_builder", "macro_recorder", NULL},
};

static const int category_count = 6;

static bool is_in_categories(const char *name) {
    for (int c = 0; c < category_count; c++)
        for (int i = 0; widget_categories[c][i]; i++)
            if (strcmp(widget_categories[c][i], name) == 0) return true;
    return false;
}

static void populate_palette(BuilderApp *app) {
    int count = widget_registry_count();
    app->palette_types = malloc(count * sizeof(char *));
    app->palette_count = 0;

    for (int c = 0; c < category_count; c++) {
        for (int i = 0; widget_categories[c][i]; i++) {
            int idx = 0;
            const char *name;
            WidgetFactory factory;
            while (widget_registry_enum(&idx, &name, &factory)) {
                if (strcmp(name, widget_categories[c][i]) == 0) {
                    app->palette_types[app->palette_count++] = strdup(name);
                    break;
                }
            }
        }
    }

    int idx = 0;
    const char *name;
    WidgetFactory factory;
    while (widget_registry_enum(&idx, &name, &factory)) {
        if (!is_in_categories(name)) {
            app->palette_types[app->palette_count++] = strdup(name);
        }
    }
}

static void update_status(BuilderApp *app) {
    free(app->status_message);
    char buf[512];
    int errors = 0;
    int warnings = 0;
    int infos = 0;
    if (app->last_report) {
        for (int i = 0; i < app->last_report->count; i++) {
            if (app->last_report->issues[i].severity == V_ERROR) errors++;
            else if (app->last_report->issues[i].severity == V_WARNING) warnings++;
            else infos++;
        }
    }

    const char *mode_str =
        app->mode == MODE_EDIT ? "Edit" :
        app->mode == MODE_WIRE ? "Wire" :
        app->mode == MODE_PREVIEW ? "Preview" : "TUI";

    snprintf(buf, sizeof(buf),
             "%s | %d items | %d nodes | %d edges | %dE %dW %dI | %s | %s",
             app->project->project_name,
             app->project->item_count,
             app->project->node_count,
             app->project->edge_count,
             errors, warnings, infos,
             mode_str,
             app->project_dirty ? "MODIFIED" : "saved");
    app->status_message = strdup(buf);
}

static void run_validation(BuilderApp *app) {
    if (app->last_report) validation_report_free(app->last_report);
    app->last_report = validator_check_all(app->project);
}

static void handle_palette_key(BuilderApp *app, KeyCode code, char ch) {
    (void)ch;
    if (code == KEY_UP && app->palette_idx > 0) {
        app->palette_idx--;
        app->base.dirty = true;
    } else if (code == KEY_DOWN && app->palette_idx < app->palette_count - 1) {
        app->palette_idx++;
        app->base.dirty = true;
    } else if (code == KEY_ENTER) {
        canvas_add_widget(app->canvas, app->palette_types[app->palette_idx],
                          app->canvas->canvas_w / 2, app->canvas->canvas_h / 2);
        CanvasItem *item = &app->project->items[app->project->item_count - 1];
        project_add_node(app->project, item->id);
        app->project_dirty = true;
        app->base.dirty = true;
    }
}

static void handle_canvas_key(BuilderApp *app, KeyCode code, char ch) {
    (void)ch;
    switch (code) {
        case KEY_UP:
            canvas_key_move(app->canvas, 0, -1);
            app->project_dirty = true;
            break;
        case KEY_DOWN:
            canvas_key_move(app->canvas, 0, 1);
            app->project_dirty = true;
            break;
        case KEY_LEFT:
            canvas_key_move(app->canvas, -1, 0);
            app->project_dirty = true;
            break;
        case KEY_RIGHT:
            canvas_key_move(app->canvas, 1, 0);
            app->project_dirty = true;
            break;
        case KEY_DELETE:
            canvas_delete_selected(app->canvas);
            app->project_dirty = true;
            break;
        default:
            break;
    }

    if (code == KEY_CHAR && ch == '+' && app->canvas->zoom < 5.0f) {
        canvas_zoom_in(app->canvas);
    }
    if (code == KEY_CHAR && ch == '-' && app->canvas->zoom > 0.1f) {
        canvas_zoom_out(app->canvas);
    }
    if (code == KEY_CHAR && ch == '0') {
        canvas_zoom_to_fit(app->canvas);
    }
    if (code == KEY_CHAR && ch == 'g') {
        canvas_toggle_grid(app->canvas);
    }
    if (code == KEY_CHAR && ch == 't') {
        canvas_toggle_tab_order(app->canvas);
    }
    if (code == KEY_CHAR && ch == 'a') {
        canvas_select_all(app->canvas);
    }

    app->base.dirty = true;
}

static void handle_prop_key(BuilderApp *app, KeyCode code, char ch) {
    prop_editor_key(app->prop_editor, code, ch);
    app->project_dirty = true;
    app->base.dirty = true;
}

static void handle_graph_key(BuilderApp *app, KeyCode code, char ch) {
    graph_key(app->graph, code, ch);
    app->project_dirty = true;
    app->base.dirty = true;
}

static void builder_render(Widget *self, Rect area, RenderTree *out) {
    BuilderApp *app = (BuilderApp *)(self + 1);
    memset(out, 0, sizeof(*out));
    out->type = RNODE_CONTAINER;
    out->rect = area;
    out->container.border = BORDER_SINGLE;
    out->container.padding = edgeinsets_zero();

    int children_count = 4;
    RenderTree *children = arena_alloc(g_session_arena, children_count * sizeof(RenderTree));

    RenderTree *toolbar = &children[0];
    memset(toolbar, 0, sizeof(*toolbar));
    toolbar->type = RNODE_TEXT;
    toolbar->rect = rect_new(0, 0, area.w, 1);
    toolbar->text.content = arena_strdup(g_session_arena,
        "F1:Edit F2:Wire F3:Preview F4:TUI Tab:Cycle Ctrl+S:Save Ctrl+E:Export Ctrl+V:Validate Del:Delete +/-:Zoom Esc:Quit");
    toolbar->style_class = "text";
    toolbar->state = "muted";

    int main_h = app->show_validation ? area.h - 8 : area.h - 3;

    RenderTree *main = &children[1];
    memset(main, 0, sizeof(*main));
    main->type = RNODE_SPLIT_PANES;
    main->rect = rect_new(0, 1, area.w, main_h);
    main->split_panes.orientation = ORIENT_HORIZONTAL;

    int left_w = area.w * 20 / 100;
    if (left_w < 20) left_w = 20;
    int center_w = area.w * 55 / 100;
    if (center_w < 40) center_w = 40;
    int right_w = area.w - left_w - center_w;
    if (right_w < 20) right_w = 20;

    main->split_panes.split_position = left_w;
    main->split_panes.split_position2 = left_w + center_w;

    RenderTree *left_child = arena_alloc(g_session_arena, sizeof(RenderTree));
    memset(left_child, 0, sizeof(*left_child));
    left_child->type = RNODE_LIST;
    left_child->rect = rect_new(0, 0, left_w, main_h);
    left_child->list.item_count = app->palette_count;
    left_child->list.selected = app->palette_idx;
    left_child->list.items = arena_alloc(g_session_arena, app->palette_count * sizeof(ListItem));
    for (int i = 0; i < app->palette_count; i++)
        left_child->list.items[i].label = arena_strdup(g_session_arena, app->palette_types[i]);
    left_child->style_class = "list";

    RenderTree *center_child = arena_alloc(g_session_arena, sizeof(RenderTree));
    memset(center_child, 0, sizeof(*center_child));
    center_child->rect = rect_new(0, 0, center_w, main_h);

    if (app->mode == MODE_PREVIEW || app->mode == MODE_TUI_PREVIEW) {
        center_child->type = RNODE_TEXT;
        center_child->rect = rect_new(0, 0, center_w, main_h);
        if (app->mode == MODE_TUI_PREVIEW) {
            center_child->text.content = arena_strdup(g_session_arena,
                "TUI Preview Mode\n\n"
                "This mode shows how your layout renders in a terminal.\n"
                "Press F1 to return to edit mode.");
        } else {
            center_child->text.content = arena_strdup(g_session_arena,
                "Live Preview Mode\n\n"
                "This is how your interface will look to users.\n"
                "Press F1 to return to edit mode.");
        }
        center_child->text.align = ALIGN_CENTER;
        center_child->style_class = "text";
    } else {
        canvas_draw_to_tree(app->canvas, center_child, 0, 0);
    }

    RenderTree *right_child = arena_alloc(g_session_arena, sizeof(RenderTree));
    memset(right_child, 0, sizeof(*right_child));
    right_child->rect = rect_new(0, 0, right_w, main_h);

    if (app->mode == MODE_WIRE) {
        graph_render(app->graph, right_child, rect_new(0, 0, right_w, main_h));
    } else {
        prop_editor_set_item(app->prop_editor, app->canvas->selected_item);
        prop_editor_render(app->prop_editor, right_child, rect_new(0, 0, right_w, main_h));
    }

    main->split_panes.first = left_child;
    main->split_panes.second = center_child;
    main->split_panes.third = right_child;

    RenderTree *graph_bar = &children[2];
    memset(graph_bar, 0, sizeof(*graph_bar));
    graph_bar->type = RNODE_TEXT;
    graph_bar->rect = rect_new(0, area.h - 5, area.w, 1);
    if (app->mode == MODE_WIRE && app->graph->wire_start_node >= 0) {
        graph_bar->text.content = arena_strdup(g_session_arena,
            "Drawing wire... click an input port to connect (Esc to cancel)");
    } else if (app->graph->editing_edge) {
        graph_bar->text.content = arena_strdup(g_session_arena,
            "Editing edge — press Enter to confirm, Esc to cancel");
    } else if (app->keymap_editing) {
        graph_bar->text.content = arena_strdup(g_session_arena,
            "Press the key to bind, or Esc to cancel");
    } else {
        graph_bar->text.content = arena_strdup(g_session_arena,
            "Right-click items for context menu | Double-click edges to edit FIL");
    }
    graph_bar->style_class = "muted";

    if (app->show_validation && app->last_report && app->last_report->count > 0) {
        RenderTree *val_pane = &children[3];
        memset(val_pane, 0, sizeof(*val_pane));
        val_pane->type = RNODE_TEXT;
        val_pane->rect = rect_new(0, area.h - 4, area.w, 3);
        char *val_text = arena_alloc(g_session_arena, 2048);
        int off = 0;
        off += snprintf(val_text + off, 2048 - off, "Validation (%d issues):\n", app->last_report->count);
        int start = app->validation_scroll;
        if (start < 0) start = 0;
        int shown = 0;
        for (int i = start; i < app->last_report->count && shown < 2; i++) {
            const char *sev = app->last_report->issues[i].severity == V_ERROR ? "ERR" :
                              app->last_report->issues[i].severity == V_WARNING ? "WARN" : "INFO";
            off += snprintf(val_text + off, 2048 - off, "  [%s] %s\n",
                           sev, app->last_report->issues[i].message);
            shown++;
        }
        val_pane->text.content = val_text;
        val_pane->style_class = "text";
    }

    RenderTree *status = &children[2];
    status->type = RNODE_TEXT;
    status->rect = rect_new(0, area.h - 1, area.w, 1);
    update_status(app);
    status->text.content = arena_strdup(g_session_arena, app->status_message);
    status->style_class = "text";
    status->state = "muted";

    out->container.children = children;
    out->container.child_count = children_count;
}

static EventResult builder_handle_event(Widget *self, Event *ev, Backend *backend) {
    (void)backend;
    BuilderApp *app = (BuilderApp *)(self + 1);

    if (app->keymap_editing) {
        if (ev->type == EVENT_KEY && ev->code == KEY_ESC) {
            app->keymap_editing = false;
            app->base.dirty = true;
            return event_result_handled();
        }
        if (ev->type == EVENT_KEY) {
            char key_name[32] = {0};
            if (ev->code == KEY_ENTER) strcpy(key_name, "Enter");
            else if (ev->code == KEY_ESC) strcpy(key_name, "Escape");
            else if (ev->code == KEY_TAB) strcpy(key_name, "Tab");
            else if (ev->code == KEY_UP) strcpy(key_name, "Up");
            else if (ev->code == KEY_DOWN) strcpy(key_name, "Down");
            else if (ev->code == KEY_LEFT) strcpy(key_name, "Left");
            else if (ev->code == KEY_RIGHT) strcpy(key_name, "Right");
            else if (ev->code == KEY_BACKSPACE) strcpy(key_name, "Backspace");
            else if (ev->code == KEY_DELETE) strcpy(key_name, "Delete");
            else if (ev->code >= KEY_F1 && ev->code <= KEY_F12)
                snprintf(key_name, sizeof(key_name), "F%d", ev->code - KEY_F1 + 1);
            else if (ev->code == KEY_CHAR && ev->ch > 0)
                snprintf(key_name, sizeof(key_name), "%c", ev->ch);

            if (key_name[0]) {
                project_add_keymap(app->project, KB_SCOPE_GLOBAL, -1, key_name, "custom");
                app->project_dirty = true;
            }
            app->keymap_editing = false;
            app->base.dirty = true;
            return event_result_handled();
        }
        return event_result_unhandled();
    }

    if (ev->type == EVENT_KEY && ev->code == KEY_CHAR) {
        switch (ev->ch) {
            case 's': case 'S':
                if (!app->file_path)
                    app->file_path = strdup("untitled.filly-project");
                project_save(app->project, app->file_path);
                app->project_dirty = false;
                run_validation(app);
                app->base.dirty = true;
                return event_result_handled();
            case 'e': case 'E':
                run_validation(app);
                if (app->last_report) {
                    bool has_errors = false;
                    for (int i = 0; i < app->last_report->count; i++)
                        if (app->last_report->issues[i].severity == V_ERROR) has_errors = true;
                    if (!has_errors) {
                        codegen_c_plugin(app->project, app->project->project_name);
                    }
                } else {
                    codegen_c_plugin(app->project, app->project->project_name);
                }
                app->base.dirty = true;
                return event_result_handled();
            case 'v': case 'V':
                run_validation(app);
                app->show_validation = !app->show_validation;
                app->validation_scroll = 0;
                app->base.dirty = true;
                return event_result_handled();
            case 'k': case 'K':
                app->keymap_editing = true;
                app->base.dirty = true;
                return event_result_handled();
        }
    }

    if (ev->type == EVENT_KEY) {
        switch (ev->code) {
            case KEY_F1:
                app->mode = MODE_EDIT;
                app->active_pane = 1;
                app->base.dirty = true;
                return event_result_handled();
            case KEY_F2:
                app->mode = MODE_WIRE;
                app->active_pane = 2;
                graph_sync_nodes(app->graph);
                app->base.dirty = true;
                return event_result_handled();
            case KEY_F3:
                app->mode = MODE_PREVIEW;
                app->base.dirty = true;
                return event_result_handled();
            case KEY_F4:
                app->mode = MODE_TUI_PREVIEW;
                app->base.dirty = true;
                return event_result_handled();
            case KEY_ESC:
                if (app->mode == MODE_WIRE && app->graph->wire_start_node >= 0) {
                    app->graph->wire_start_node = -1;
                    app->graph->wire_start_port = -1;
                } else if (app->prop_editor->editing) {
                    app->prop_editor->editing = false;
                    app->prop_editor->field_len = 0;
                    app->prop_editor->field_buf[0] = '\0';
                } else if (app->show_validation) {
                    app->show_validation = false;
                } else {
                    if (app->project_dirty) {
                        project_save(app->project, app->file_path ? app->file_path : "untitled.filly-project");
                    }
                    return event_result_response((WidgetResponse){NULL, true, NULL});
                }
                app->base.dirty = true;
                return event_result_handled();
            case KEY_TAB:
                app->active_pane = (app->active_pane + 1) % 3;
                app->base.dirty = true;
                return event_result_handled();
            case KEY_UP:
                if (app->show_validation && app->validation_scroll > 0) {
                    app->validation_scroll--;
                    app->base.dirty = true;
                    return event_result_handled();
                }
                break;
            case KEY_DOWN:
                if (app->show_validation && app->last_report &&
                    app->validation_scroll < app->last_report->count - 1) {
                    app->validation_scroll++;
                    app->base.dirty = true;
                    return event_result_handled();
                }
                break;
            default:
                break;
        }
    }

    if (app->active_pane == 0) {
        if (ev->type == EVENT_KEY) handle_palette_key(app, ev->code, ev->ch);
    } else if (app->active_pane == 1 && app->mode == MODE_EDIT) {
        if (ev->type == EVENT_KEY) {
            handle_canvas_key(app, ev->code, ev->ch);
        } else if (ev->type == EVENT_MOUSE_BUTTON && ev->mouse_state == MOUSE_PRESS) {
            canvas_mouse_down(app->canvas, ev->x, ev->y, ev->button);
            if (app->canvas->selected_item >= 0) {
                prop_editor_set_item(app->prop_editor, app->canvas->selected_item);
            }
            app->base.dirty = true;
            return event_result_handled();
        } else if (ev->type == EVENT_MOUSE_MOTION || ev->type == EVENT_MOUSE_DRAG_MOVE) {
            canvas_mouse_move(app->canvas, ev->x, ev->y,
                ev->mouse_state == MOUSE_PRESS);
            app->base.dirty = true;
            return event_result_handled();
        } else if (ev->type == EVENT_MOUSE_BUTTON && ev->mouse_state == MOUSE_RELEASE) {
            canvas_mouse_up(app->canvas, ev->x, ev->y, ev->button);
            app->base.dirty = true;
            return event_result_handled();
        } else if (ev->type == EVENT_MOUSE_SCROLL) {
            canvas_mouse_scroll(app->canvas, ev->x, ev->y,
                ev->mouse_state == MOUSE_SCROLL_UP ? 1 : -1);
            app->base.dirty = true;
            return event_result_handled();
        }
    } else if (app->active_pane == 2) {
        if (app->mode == MODE_WIRE) {
            if (ev->type == EVENT_KEY) {
                handle_graph_key(app, ev->code, ev->ch);
            } else if (ev->type == EVENT_MOUSE_BUTTON && ev->mouse_state == MOUSE_PRESS) {
                graph_mouse_down(app->graph, ev->x, ev->y);
                app->base.dirty = true;
                return event_result_handled();
            } else if (ev->type == EVENT_MOUSE_MOTION) {
                graph_mouse_move(app->graph, ev->x, ev->y);
                app->base.dirty = true;
                return event_result_handled();
            } else if (ev->type == EVENT_MOUSE_BUTTON && ev->mouse_state == MOUSE_RELEASE) {
                graph_mouse_up(app->graph, ev->x, ev->y);
                app->base.dirty = true;
                return event_result_handled();
            }
        } else {
            if (ev->type == EVENT_KEY) handle_prop_key(app, ev->code, ev->ch);
        }
    }

    return event_result_unhandled();
}

static void builder_destroy(Widget *self) {
    BuilderApp *app = (BuilderApp *)(self + 1);
    if (app->last_report) validation_report_free(app->last_report);
    if (app->project) project_free(app->project);
    if (app->canvas) canvas_free(app->canvas);
    if (app->graph) graph_free(app->graph);
    if (app->prop_editor) prop_editor_free(app->prop_editor);
    free(app->status_message);
    free(app->file_path);
    for (int i = 0; i < app->palette_count; i++) free(app->palette_types[i]);
    free(app->palette_types);
}

int main(int argc, char **argv) {
    const char *project_path = NULL;
    const char *output_dir = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--project") == 0 && i + 1 < argc) project_path = argv[++i];
        else if (strcmp(argv[i], "--export") == 0 && i + 1 < argc) output_dir = argv[++i];
    }

    register_builtin_widgets();
    gcore_init_font(NULL, 14);
    g_active_theme = theme_load("themes/forge.json");
    if (!g_active_theme) g_active_theme = theme_load_directory("themes");
    if (!g_active_theme) g_active_theme = theme_default();

    BuilderApp *app = calloc(1, sizeof(BuilderApp));

    if (project_path) {
        app->project = project_load(project_path);
        if (!app->project)
            app->project = project_new("untitled", 800, 600);
        app->file_path = strdup(project_path);
    } else {
        app->project = project_new("untitled", 800, 600);
    }

    app->canvas = canvas_new(app->project, 800, 600);
    app->graph = graph_new(app->project);
    app->prop_editor = prop_editor_new(app->project);
    app->mode = MODE_EDIT;
    app->active_pane = 1;
    populate_palette(app);
    run_validation(app);

    if (output_dir) {
        CodegenResult result = codegen_c_plugin(app->project, output_dir);
        for (int i = 0; i < result.error_count; i++)
            fprintf(stderr, "Error: %s\n", result.errors[i]);
        for (int i = 0; i < result.warning_count; i++)
            fprintf(stderr, "Warning: %s\n", result.warnings[i]);
        codegen_result_free(&result);
        project_free(app->project);
        canvas_free(app->canvas);
        graph_free(app->graph);
        prop_editor_free(app->prop_editor);
        for (int i = 0; i < app->palette_count; i++) free(app->palette_types[i]);
        free(app->palette_types);
        free(app);
        return result.error_count > 0 ? 1 : 0;
    }

    Widget *root = calloc(1, sizeof(Widget) + sizeof(BuilderApp));
    memcpy(root + 1, app, sizeof(BuilderApp));
    free(app);
    root->vtable.render = builder_render;
    root->vtable.handle_event = builder_handle_event;
    root->vtable.is_dirty = widget_base_is_dirty;
    root->vtable.clear_dirty = widget_base_clear_dirty;
    root->vtable.destroy = builder_destroy;

#ifdef FILLY_GCORE
    if (getenv("WAYLAND_DISPLAY") || getenv("DISPLAY")) {
        GCoreBackend g;
        if (gcore_backend_init(&g, GCORE_DRM, NULL)) {
            Backend bg = { .vtable = &gcore_vtable, .data = &g };
            session_run(root, &bg);
            gcore_backend_destroy(&g);
            return 0;
        }
    }
#endif

    HeadlessBackend hl;
    headless_backend_init(&hl, 80, 24);
    Backend backend = { .vtable = &headless_vtable, .data = &hl };
    session_run(root, &backend);
    headless_backend_destroy(&hl);
    return 0;
}