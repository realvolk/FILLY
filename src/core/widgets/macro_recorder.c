#include "macro_recorder.h"
#include "core/widget_base.h"
#include "core/session.h"
#include "core/recorder.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef enum { MR_BROWSING, MR_SAVE_INPUT } MacroRecorderMode;

typedef struct {
    WidgetBase base;
    bool recording;
    char *save_path;
    MacroRecorderMode mode;
} MacroRecorderData;

extern Arena *g_session_arena;

static void mr_render(Widget *self, Rect area, RenderTree *out) {
    MacroRecorderData *d = (MacroRecorderData *)(self + 1);
    memset(out, 0, sizeof(*out));
    out->style_class = "container";

    int box_w = 50, box_h = 8;
    if (box_w > area.w - 2) box_w = area.w - 2;
    if (box_h > area.h - 2) box_h = area.h - 2;

    RenderTree *children = arena_alloc(g_session_arena, 4 * sizeof(RenderTree));

    children[0].type = RNODE_TEXT;
    children[0].rect = rect_new(1, 0, box_w - 2, 1);
    children[0].text.content = arena_strdup(g_session_arena, "Macro Recorder");
    children[0].style_class = "text";
    children[0].state = "title";

    children[1].type = RNODE_TEXT;
    children[1].rect = rect_new(1, 1, box_w - 2, 1);
    children[1].text.content = d->recording ? "Status: RECORDING" : "Status: STOPPED";
    children[1].style_class = "text";

    children[2].type = RNODE_TEXT;
    children[2].rect = rect_new(1, 3, box_w - 2, 2);
    children[2].text.content = arena_strdup(g_session_arena,
        d->mode == MR_SAVE_INPUT ? "Enter filename to save:" :
        "R:record  S:stop  P:play  L:load  W:save  Esc:quit");
    children[2].style_class = "text";

    children[3].type = RNODE_TEXT;
    children[3].rect = rect_new(1, 5, box_w - 2, 1);
    if (d->mode == MR_SAVE_INPUT && d->save_path) {
        char buf[256];
        snprintf(buf, sizeof(buf), "> %s", d->save_path);
        children[3].text.content = arena_strdup(g_session_arena, buf);
    } else {
        children[3].text.content = arena_strdup(g_session_arena, "");
    }
    children[3].style_class = "text";

    out->type = RNODE_CONTAINER;
    out->rect = rect_new((area.w - box_w) / 2, (area.h - box_h) / 2, box_w, box_h);
    out->container.border = BORDER_SINGLE;
    out->container.padding = edgeinsets_zero();
    out->container.children = children;
    out->container.child_count = 4;
}

static EventResult mr_handle_event(Widget *self, Event *ev, Backend *backend) {
    (void)backend;
    MacroRecorderData *d = (MacroRecorderData *)(self + 1);

    if (ev->type != EVENT_KEY) return event_result_unhandled();

    if (d->mode == MR_SAVE_INPUT) {
        switch (ev->code) {
            case KEY_ESC:
                d->mode = MR_BROWSING;
                d->base.dirty = true;
                return event_result_handled();
            case KEY_ENTER:
                if (d->save_path && strlen(d->save_path) > 0) {
                    recorder_save(d->save_path);
                }
                d->mode = MR_BROWSING;
                d->base.dirty = true;
                return event_result_handled();
            case KEY_BACKSPACE:
                if (d->save_path && strlen(d->save_path) > 0)
                    d->save_path[strlen(d->save_path) - 1] = '\0';
                d->base.dirty = true;
                return event_result_handled();
            case KEY_CHAR:
                if (ev->ch >= 32) {
                    int len = d->save_path ? strlen(d->save_path) : 0;
                    d->save_path = realloc(d->save_path, len + 2);
                    d->save_path[len] = ev->ch;
                    d->save_path[len + 1] = '\0';
                }
                d->base.dirty = true;
                return event_result_handled();
            default:
                return event_result_unhandled();
        }
    }

    switch (ev->code) {
        case KEY_ESC:
            return event_result_response((WidgetResponse){ .result = NULL, .cancelled = true });
        case KEY_CHAR:
            if (ev->ch == 'r' || ev->ch == 'R') {
                if (!d->recording) { recorder_start(); d->recording = true; }
            } else if (ev->ch == 's' || ev->ch == 'S') {
                if (d->recording) { recorder_stop(); d->recording = false; }
            } else if (ev->ch == 'w' || ev->ch == 'W') {
                d->mode = MR_SAVE_INPUT;
                free(d->save_path);
                d->save_path = strdup("");
            } else if (ev->ch == 'l' || ev->ch == 'L') {
                d->mode = MR_SAVE_INPUT;
                free(d->save_path);
                d->save_path = strdup("");
            } else if (ev->ch == 'p' || ev->ch == 'P') {
                if (!d->recording && d->save_path) {
                    recorder_load(d->save_path);
                }
            }
            d->base.dirty = true;
            return event_result_handled();
        default:
            return event_result_unhandled();
    }
}

static void mr_destroy(Widget *self) {
    MacroRecorderData *d = (MacroRecorderData *)(self + 1);
    free(d->save_path);
}

Widget *macro_recorder_widget_new(void) {
    Widget *w = calloc(1, sizeof(Widget) + sizeof(MacroRecorderData));
    MacroRecorderData *d = (MacroRecorderData *)(w + 1);
    d->base.dirty = true;
    d->recording = false;
    d->save_path = strdup("");
    d->mode = MR_BROWSING;
    w->vtable.render = mr_render;
    w->vtable.handle_event = mr_handle_event;
    w->vtable.is_dirty = widget_base_is_dirty;
    w->vtable.clear_dirty = widget_base_clear_dirty;
    w->vtable.destroy = mr_destroy;
    return w;
}