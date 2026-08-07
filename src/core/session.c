#include "session.h"
#include "render.h"
#include "theme.h"
#include "arena.h"
#include "log.h"
#include "widget_base.h"
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include "script/fil.h"
#include "core/accessibility.h"
#include "core/animation.h"
#include <stdio.h>

extern BackendVTable gcore_vtable;
extern BackendVTable headless_pixel_vtable;

Theme *g_active_theme = NULL;
Store *g_active_store = NULL;
double session_current_fps = 0;
Arena *g_session_arena = NULL;
ClipboardInterface *session_clipboard = NULL;
bool session_prefers_reduced_motion = false;

char *focused_widget_id = NULL;
char *tooltip_target_id = NULL;
int tooltip_hover_x = 0;
int tooltip_hover_y = 0;

typedef struct { char *key; KeyCode code; char ch; } KeyBinding;
static KeyBinding *keymap = NULL;
static int keymap_count = 0;
static UndoStack *session_undo = NULL;
static RenderTree *last_render_tree = NULL;
static long long tooltip_hover_start = 0;

#ifdef FILLY_PROFILING
static int prof_frame_count = 0;
static long long prof_fps_start = 0;
#endif

static long long time_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long long)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

static void load_default_keymap(void) {
    session_clear_keymap();
    session_load_keymap("j", KEY_DOWN, 0);
    session_load_keymap("k", KEY_UP, 0);
    session_load_keymap("q", KEY_ESC, 0);
    session_load_keymap("h", KEY_LEFT, 0);
    session_load_keymap("l", KEY_RIGHT, 0);
    session_load_keymap("i", KEY_ENTER, 0);
    session_load_keymap("\t", KEY_TAB, 0);
}

static bool is_printable_paste(const char *text) {
    for (const char *c = text; *c; c++) {
        unsigned char ch = (unsigned char)*c;
        if (ch < 32 && ch != '\n' && ch != '\t' && ch != '\r') return false;
        if (ch == 127) return false;
    }
    return true;
}

static void mark_dirty(Widget *w) {
    WidgetBase *base = (WidgetBase *)((char *)w + sizeof(Widget));
    base->dirty = true;
}

static RenderTree *find_focusable(RenderTree *tree, int target_tab_index, int *current_tab, RenderTree **found) {
    if (!tree || *found) return NULL;
    if (tree->tab_index >= 0) {
        if (*current_tab == target_tab_index) {
            *found = tree;
            return tree;
        }
        (*current_tab)++;
    }
    if (tree->type == RNODE_CONTAINER && tree->u.container.children) {
        for (int i = 0; i < tree->u.container.child_count && !*found; i++)
            find_focusable(&tree->u.container.children[i], target_tab_index, current_tab, found);
    }
    if (tree->type == RNODE_FLEX && tree->u.flex.children) {
        for (int i = 0; i < tree->u.flex.child_count && !*found; i++)
            find_focusable(&tree->u.flex.children[i], target_tab_index, current_tab, found);
    }
    if (tree->type == RNODE_GRID && tree->u.grid.children) {
        for (int i = 0; i < tree->u.grid.child_count && !*found; i++)
            find_focusable(&tree->u.grid.children[i], target_tab_index, current_tab, found);
    }
    if (tree->type == RNODE_TABS && tree->u.tabs.child)
        find_focusable(tree->u.tabs.child, target_tab_index, current_tab, found);
    if (tree->type == RNODE_SPLIT_PANES) {
        if (tree->u.split_panes.first) find_focusable(tree->u.split_panes.first, target_tab_index, current_tab, found);
        if (!*found && tree->u.split_panes.second) find_focusable(tree->u.split_panes.second, target_tab_index, current_tab, found);
        if (!*found && tree->u.split_panes.third) find_focusable(tree->u.split_panes.third, target_tab_index, current_tab, found);
    }
    return *found;
}

static int count_focusable(RenderTree *tree) {
    if (!tree) return 0;
    int count = tree->tab_index >= 0 ? 1 : 0;
    if (tree->type == RNODE_CONTAINER && tree->u.container.children)
        for (int i = 0; i < tree->u.container.child_count; i++)
            count += count_focusable(&tree->u.container.children[i]);
    if (tree->type == RNODE_FLEX && tree->u.flex.children)
        for (int i = 0; i < tree->u.flex.child_count; i++)
            count += count_focusable(&tree->u.flex.children[i]);
    if (tree->type == RNODE_GRID && tree->u.grid.children)
        for (int i = 0; i < tree->u.grid.child_count; i++)
            count += count_focusable(&tree->u.grid.children[i]);
    if (tree->type == RNODE_TABS && tree->u.tabs.child)
        count += count_focusable(tree->u.tabs.child);
    if (tree->type == RNODE_SPLIT_PANES) {
        if (tree->u.split_panes.first) count += count_focusable(tree->u.split_panes.first);
        if (tree->u.split_panes.second) count += count_focusable(tree->u.split_panes.second);
        if (tree->u.split_panes.third) count += count_focusable(tree->u.split_panes.third);
    }
    return count;
}

static RenderTree *hit_test(RenderTree *node, int px, int py, int off_x, int off_y) {
    if (!node) return NULL;
    int x = off_x + node->rect.x, y = off_y + node->rect.y, w = node->rect.w, h = node->rect.h;
    if (px < x || px >= x + w || py < y || py >= y + h) return NULL;
    RenderTree *hit = NULL;
    if (node->type == RNODE_CONTAINER && node->u.container.children) {
        for (int i = node->u.container.child_count - 1; i >= 0; i--) {
            hit = hit_test(&node->u.container.children[i], px, py, x, y);
            if (hit) return hit;
        }
    }
    if (node->type == RNODE_FLEX && node->u.flex.children) {
        for (int i = node->u.flex.child_count - 1; i >= 0; i--) {
            hit = hit_test(&node->u.flex.children[i], px, py, x, y);
            if (hit) return hit;
        }
    }
    if (node->type == RNODE_GRID && node->u.grid.children) {
        for (int i = node->u.grid.child_count - 1; i >= 0; i--) {
            hit = hit_test(&node->u.grid.children[i], px, py, x, y);
            if (hit) return hit;
        }
    }
    return node;
}

static void compute_delta_recursive(RenderTree *cur, RenderTree *prev, DirtyNode **nodes, int *count, int *capacity) {
    if (!cur || !prev) return;
    if (cur->type != prev->type) return;
    
    bool differs = false;
    if (cur->rect.x != prev->rect.x || cur->rect.y != prev->rect.y ||
        cur->rect.w != prev->rect.w || cur->rect.h != prev->rect.h) differs = true;
    if (cur->dirty) differs = true;
    
    if (differs) {
        if (*count >= *capacity) {
            *capacity = *capacity ? *capacity * 2 : 16;
            *nodes = realloc(*nodes, *capacity * sizeof(DirtyNode));
        }
        (*nodes)[*count].id = cur->type;
        (*nodes)[*count].rect = cur->rect;
        (*count)++;
    }
    
    if (cur->type == RNODE_CONTAINER && cur->u.container.children && prev->u.container.children) {
        int min_children = cur->u.container.child_count < prev->u.container.child_count ?
                          cur->u.container.child_count : prev->u.container.child_count;
        for (int i = 0; i < min_children; i++)
            compute_delta_recursive(&cur->u.container.children[i], &prev->u.container.children[i], nodes, count, capacity);
    }
    if (cur->type == RNODE_FLEX && cur->u.flex.children && prev->u.flex.children) {
        int min_children = cur->u.flex.child_count < prev->u.flex.child_count ?
                          cur->u.flex.child_count : prev->u.flex.child_count;
        for (int i = 0; i < min_children; i++)
            compute_delta_recursive(&cur->u.flex.children[i], &prev->u.flex.children[i], nodes, count, capacity);
    }
    if (cur->type == RNODE_GRID && cur->u.grid.children && prev->u.grid.children) {
        int min_children = cur->u.grid.child_count < prev->u.grid.child_count ?
                          cur->u.grid.child_count : prev->u.grid.child_count;
        for (int i = 0; i < min_children; i++)
            compute_delta_recursive(&cur->u.grid.children[i], &prev->u.grid.children[i], nodes, count, capacity);
    }
}

FrameDelta session_compute_delta(RenderTree *current, RenderTree *previous) {
    FrameDelta delta = {0};
    if (!current || !previous) return delta;
    int capacity = 0;
    compute_delta_recursive(current, previous, &delta.nodes, &delta.count, &capacity);
    return delta;
}

void session_free_delta(FrameDelta *delta) {
    if (!delta) return;
    free(delta->nodes);
    delta->nodes = NULL;
    delta->count = 0;
}

WidgetResponse session_run(Widget *w, Backend *backend) {
    if (!w || !backend || !backend->vtable) {
        WidgetResponse err = { .result = NULL, .cancelled = true, .error = "Invalid widget or backend" };
        return err;
    }
    if (!w->vtable.render || !w->vtable.handle_event ||
        !w->vtable.is_dirty || !w->vtable.clear_dirty) {
        WidgetResponse err = { .result = NULL, .cancelled = true, .error = "Incomplete widget vtable" };
        return err;
    }
    if (!session_undo) session_undo = undo_stack_new(100);
    load_default_keymap();
    backend->vtable->setup(backend->data);
    int term_w, term_h;
    backend->vtable->get_size(backend->data, &term_w, &term_h);
    int last_w = term_w, last_h = term_h;
    int idle_count = 0, last_store_version = 0;
    long long last_frame = 0;
    g_session_arena = arena_new(1024 * 1024);
    bool is_pixel = (backend->vtable == &gcore_vtable || backend->vtable == &headless_pixel_vtable);

    WidgetBase *wb = (WidgetBase *)((char *)w + sizeof(Widget));
    wb->render_area = rect_new(0, 0, term_w, term_h);

    const char *reduced = getenv("FILLY_REDUCED_MOTION");
    session_prefers_reduced_motion = reduced && (strcmp(reduced, "1") == 0 || strcmp(reduced, "true") == 0);

#ifdef FILLY_PROFILING
    if (prof_fps_start == 0) prof_fps_start = time_ms();
#endif

    {
        arena_reset(g_session_arena);
        RenderTree tree;
        memset(&tree, 0, sizeof(tree));
        w->vtable.render(w, &tree);
        if (g_active_theme) resolve_node_styles(&tree, g_active_theme);
        layout_tree(&tree, term_w, term_h, is_pixel);
        long long now = time_ms();
        animation_update(&tree, now, session_prefers_reduced_motion);
        backend->vtable->draw(backend->data, &tree);
        accessibility_push_tree(&tree, focused_widget_id);
        w->vtable.clear_dirty(w);
        last_frame = time_ms();
        if (last_render_tree) render_tree_free(last_render_tree);
    }

    while (1) {
        backend->vtable->get_size(backend->data, &term_w, &term_h);
        if (term_w != last_w || term_h != last_h) {
            mark_dirty(w);
            wb->render_area.w = term_w;
            wb->render_area.h = term_h;
            last_w = term_w;
            last_h = term_h;
        }

        if (w->vtable.is_dirty(w)) {
            arena_reset(g_session_arena);
            RenderTree tree;
            memset(&tree, 0, sizeof(tree));
            w->vtable.render(w, &tree);
            if (g_active_theme) resolve_node_styles(&tree, g_active_theme);
            layout_tree(&tree, term_w, term_h, is_pixel);
            long long now = time_ms();
            animation_update(&tree, now, session_prefers_reduced_motion);
            backend->vtable->draw(backend->data, &tree);
            accessibility_push_tree(&tree, focused_widget_id);
            w->vtable.clear_dirty(w);
            last_frame = time_ms();
            if (last_render_tree) render_tree_free(last_render_tree);
#ifdef FILLY_PROFILING
            prof_frame_count++;
            if (now - prof_fps_start >= 1000) {
                session_current_fps = prof_frame_count * 1000.0 / (now - prof_fps_start);
                prof_frame_count = 0;
                prof_fps_start = now;
            }
#endif
        }
        Event ev = backend->vtable->next_event(backend->data);
        if (ev.type == EVENT_NONE) {
            if (backend->vtable->is_interactive) {
                long long now = time_ms();
                if (tooltip_target_id && (now - tooltip_hover_start) >= 500) {
                    mark_dirty(w);
                    tooltip_hover_start = now;
                }
                continue;
            }
            idle_count++;
            if (idle_count > 5000) {
                WidgetResponse timeout = { .result = NULL, .cancelled = false };
                backend->vtable->teardown(backend->data);
                arena_free(g_session_arena);
                g_session_arena = NULL;
                free(focused_widget_id); focused_widget_id = NULL;
                free(tooltip_target_id); tooltip_target_id = NULL;
                return timeout;
            }
            continue;
        }
        if (ev.type == EVENT_RESIZE) continue;

        if (ev.type == EVENT_MOUSE_MOTION) {
            tooltip_target_id = NULL;
            tooltip_hover_start = 0;
            RenderTree *hit = hit_test(last_render_tree, ev.x, ev.y, 0, 0);
            if (hit && hit->tooltip && hit->tooltip[0]) {
                free(tooltip_target_id);
                tooltip_target_id = strdup(hit->tooltip);
                tooltip_hover_x = ev.x;
                tooltip_hover_y = ev.y;
                tooltip_hover_start = time_ms();
            }
            mark_dirty(w);
            continue;
        }

        if (ev.type == EVENT_MOUSE_BUTTON && ev.mouse_state == MOUSE_PRESS && ev.button == 3) {
            RenderTree *hit = hit_test(last_render_tree, ev.x, ev.y, 0, 0);
            if (hit && hit->type == RNODE_CONTAINER) {
                Event cev = { .type = EVENT_KEY, .code = KEY_F2 };
                w->vtable.handle_event(w, &cev, backend);
            }
            continue;
        }

        if (ev.type == EVENT_MOUSE_DRAG_START) {
            mark_dirty(w);
            continue;
        }
        if (ev.type == EVENT_MOUSE_DRAG_MOVE) {
            mark_dirty(w);
            continue;
        }
        if (ev.type == EVENT_MOUSE_DRAG_END) {
            mark_dirty(w);
            continue;
        }

        if (ev.type == EVENT_KEY && ev.code == KEY_CHAR && ev.ch == 3) {
            if (backend->vtable->copy_to_clipboard && session_clipboard &&
                session_clipboard->has_clipboard && session_clipboard->has_clipboard(session_clipboard->data)) {
                char *text = session_clipboard->get_clipboard(session_clipboard->data);
                if (text) {
                    backend->vtable->copy_to_clipboard(backend->data, text);
                    free(text);
                }
            }
            continue;
        }
        if (ev.type == EVENT_KEY && ev.code == KEY_CHAR && ev.ch == 22) {
            if (session_clipboard && session_clipboard->has_clipboard && session_clipboard->has_clipboard(session_clipboard->data)) {
                char *text = session_clipboard->get_clipboard(session_clipboard->data);
                if (text) {
                    if (is_printable_paste(text)) {
                        for (char *c = text; *c; c++) {
                            Event pe = { .type = EVENT_KEY, .code = KEY_CHAR, .ch = *c };
                            w->vtable.handle_event(w, &pe, backend);
                        }
                    } else LOG_WARN("Clipboard paste blocked: non-printable chars");
                    free(text);
                }
            }
            mark_dirty(w);
            continue;
        }
        if (ev.type == EVENT_KEY && ev.code == KEY_CHAR && ev.ch == 26) {
            if (undo_stack_undo(session_undo)) { mark_dirty(w); continue; }
        }
        if (ev.type == EVENT_KEY && ev.code == KEY_CHAR && ev.ch == 25) {
            if (undo_stack_redo(session_undo)) { mark_dirty(w); continue; }
        }

        if (ev.type == EVENT_KEY && ev.code == KEY_TAB) {
            int total = count_focusable(last_render_tree);
            if (total > 0) {
                int current = 0;
                RenderTree *found = NULL;
                if (focused_widget_id) {
                    int dummy = 0;
                    find_focusable(last_render_tree, -1, &dummy, NULL);
                    current = dummy;
                }
                int next = (current + 1) % total;
                int idx = 0;
                find_focusable(last_render_tree, next, &idx, &found);
                if (found && found->style_class) {
                    free(focused_widget_id);
                    focused_widget_id = strdup(found->style_class);
                    mark_dirty(w);
                }
            }
            continue;
        }

        if (ev.type == EVENT_KEY) {
            if (!wb->accepts_text_input) {
                for (int i = 0; i < keymap_count; i++) {
                    if (strlen(keymap[i].key) == 1 && ev.code == KEY_CHAR && ev.ch == keymap[i].key[0]) {
                        ev.code = keymap[i].code;
                        ev.ch = keymap[i].ch;
                        break;
                    }
                }
            }
        }
        idle_count = 0;
        EventResult result = w->vtable.handle_event(w, &ev, backend);

        if (result.type == EVENT_RESULT_RESPONSE) {
            backend->vtable->teardown(backend->data);
            arena_free(g_session_arena);
            g_session_arena = NULL;
            free(focused_widget_id); focused_widget_id = NULL;
            free(tooltip_target_id); tooltip_target_id = NULL;
            return result.response;
        }

        if (g_active_store) {
            int ver;
            store_get_version(g_active_store, &ver);
            if (ver != last_store_version) {
                mark_dirty(w);
                last_store_version = ver;
            }
        }

        if (backend->vtable->wait_frame) {
            long long now = time_ms();
            if (now - last_frame < 16) backend->vtable->wait_frame(backend->data);
        }
    }
}

void session_set_clipboard(ClipboardInterface *ci) { session_clipboard = ci; }
ClipboardInterface *session_get_clipboard(void) { return session_clipboard; }
UndoStack *session_get_undo(void) {
    if (!session_undo) session_undo = undo_stack_new(100);
    return session_undo;
}
void session_load_keymap(const char *key, KeyCode code, char ch) {
    keymap = realloc(keymap, (keymap_count + 1) * sizeof(KeyBinding));
    keymap[keymap_count].key = strdup(key);
    keymap[keymap_count].code = code;
    keymap[keymap_count].ch = ch;
    keymap_count++;
}
void session_clear_keymap(void) {
    for (int i = 0; i < keymap_count; i++) free(keymap[i].key);
    free(keymap);
    keymap = NULL;
    keymap_count = 0;
}

void session_load_keymap_from_fil(FilResult *fr) {
    if (!fr || fr->keymap_count == 0) return;
    for (int i = 0; i < fr->keymap_count; i++) {
        char *entry = fr->keymap_bindings[i];
        char *scope = entry;
        char *pipe1 = strchr(scope, '|');
        if (!pipe1) continue;
        *pipe1 = '\0';
        char *key = pipe1 + 1;
        char *pipe2 = strchr(key, '|');
        if (!pipe2) { *pipe1 = '|'; continue; }
        *pipe2 = '\0';
        char *action = pipe2 + 1;

        KeyCode code = KEY_NULL;
        if (strcmp(key, "Escape") == 0) code = KEY_ESC;
        else if (strcmp(key, "Enter") == 0) code = KEY_ENTER;
        else if (strcmp(key, "Up") == 0) code = KEY_UP;
        else if (strcmp(key, "Down") == 0) code = KEY_DOWN;
        else if (strcmp(key, "Left") == 0) code = KEY_LEFT;
        else if (strcmp(key, "Right") == 0) code = KEY_RIGHT;
        else if (strcmp(key, "Tab") == 0) code = KEY_TAB;
        else if (strlen(key) == 1) code = KEY_CHAR;

        if (code != KEY_NULL) {
            char ch = (code == KEY_CHAR) ? key[0] : 0;
            if (strcmp(action, "back") == 0) { code = KEY_ESC; ch = 0; }
            else if (strcmp(action, "select") == 0) code = KEY_ENTER;
            else if (strcmp(action, "move-up") == 0) code = KEY_UP;
            else if (strcmp(action, "move-down") == 0) code = KEY_DOWN;
            else if (strcmp(action, "move-left") == 0) code = KEY_LEFT;
            else if (strcmp(action, "move-right") == 0) code = KEY_RIGHT;
            else if (strcmp(action, "confirm") == 0) code = KEY_ENTER;
            else if (strcmp(action, "cancel") == 0) code = KEY_ESC;
            session_load_keymap(key, code, ch);
        }

        *pipe1 = '|';
        if (pipe2) *pipe2 = '|';
    }
}