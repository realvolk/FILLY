#include "session.h"
#include "render.h"
#include "theme.h"
#include "arena.h"
#include "log.h"
#include "widget_base.h"
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

Theme *g_active_theme = NULL;
Store *g_active_store = NULL;
double session_current_fps = 0;
Arena *g_session_arena = NULL;
ClipboardInterface *session_clipboard = NULL;

typedef struct { char *key; KeyCode code; char ch; } KeyBinding;
static KeyBinding *keymap = NULL;
static int keymap_count = 0;
static UndoStack *session_undo = NULL;
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
#ifdef FILLY_PROFILING
    if (prof_fps_start == 0) prof_fps_start = time_ms();
#endif

    {
        arena_reset(g_session_arena);
        RenderTree tree;
        memset(&tree, 0, sizeof(tree));
        w->vtable.render(w, rect_new(0, 0, term_w, term_h), &tree);
        if (g_active_theme) resolve_node_styles(&tree, g_active_theme);
        backend->vtable->draw(backend->data, &tree);
        w->vtable.clear_dirty(w);
        last_frame = time_ms();
    }

    while (1) {
        backend->vtable->get_size(backend->data, &term_w, &term_h);
        if (term_w != last_w || term_h != last_h) {
            mark_dirty(w);
            last_w = term_w;
            last_h = term_h;
        }

        if (w->vtable.is_dirty(w)) {
            arena_reset(g_session_arena);
            RenderTree tree;
            memset(&tree, 0, sizeof(tree));
            w->vtable.render(w, rect_new(0, 0, term_w, term_h), &tree);
            if (g_active_theme) resolve_node_styles(&tree, g_active_theme);
            backend->vtable->draw(backend->data, &tree);
            w->vtable.clear_dirty(w);
            last_frame = time_ms();
#ifdef FILLY_PROFILING
            prof_frame_count++;
            long long now = time_ms();
            if (now - prof_fps_start >= 1000) {
                session_current_fps = prof_frame_count * 1000.0 / (now - prof_fps_start);
                prof_frame_count = 0;
                prof_fps_start = now;
            }
#endif
        }
        Event ev = backend->vtable->next_event(backend->data);
        if (ev.type == EVENT_NONE) {
            if (backend->vtable->is_interactive) continue;
            idle_count++;
            if (idle_count > 5000) {
                WidgetResponse timeout = { .result = NULL, .cancelled = false };
                backend->vtable->teardown(backend->data);
                arena_free(g_session_arena);
                g_session_arena = NULL;
                return timeout;
            }
            continue;
        }
        if (ev.type == EVENT_RESIZE || ev.type == EVENT_MOUSE_MOTION) {
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
        if (ev.type == EVENT_KEY) {
            WidgetBase *wb = (WidgetBase *)((char *)w + sizeof(Widget));
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