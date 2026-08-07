#include "canvas.h"
#include "core/widget_base.h"
#include "core/session.h"
#include "script/fil.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct {
    WidgetBase base;
    char *script;
    int width;
    int height;
    uint32_t *pixels;
    bool drawn;
} CanvasData;

extern Arena *g_session_arena;

static void canvas_render(Widget *self, RenderTree *out) {
    CanvasData *d = (CanvasData *)(self + 1);
    WidgetBase *base = (WidgetBase *)(self + 1);
    Rect area = base->render_area;
    memset(out, 0, sizeof(*out));

    int cw = d->width > 0 ? d->width : area.w;
    int ch = d->height > 0 ? d->height : area.h;

    if (!d->drawn && d->script && d->script[0]) {
        if (!d->pixels) {
            d->pixels = calloc(cw * ch, sizeof(uint32_t));
            for (int i = 0; i < cw * ch; i++)
                d->pixels[i] = 0xFFFFFFFF;
        }
        d->drawn = true;

        FilResult *fr = fil_eval(d->script, NULL, NULL);
        if (fr) {
            if (fr->accepted) {
                for (int i = 0; i < fr->set_count; i++) {
                    if (strncmp(fr->set_keys[i], "canvas.", 7) == 0) {
                        const char *cmd = fr->set_keys[i] + 7;
                        const char *val = fr->set_vals[i];

                        if (strcmp(cmd, "rect") == 0) {
                            int rx, ry, rw, rh, rr = 0, rg = 0, rb = 0;
                            sscanf(val, "%d,%d,%d,%d,%d,%d,%d", &rx, &ry, &rw, &rh, &rr, &rg, &rb);
                            if (rw > 0 && rh > 0) {
                                uint32_t color = (255 << 24) | ((rr & 0xFF) << 16) | ((rg & 0xFF) << 8) | (rb & 0xFF);
                                for (int row = ry; row < ry + rh && row < ch; row++)
                                    for (int col = rx; col < rx + rw && col < cw; col++)
                                        if (row >= 0 && col >= 0)
                                            d->pixels[row * cw + col] = color;
                            }
                        } else if (strcmp(cmd, "line") == 0) {
                            int x1, y1, x2, y2, lr = 0, lg = 0, lb = 0;
                            sscanf(val, "%d,%d,%d,%d,%d,%d,%d", &x1, &y1, &x2, &y2, &lr, &lg, &lb);
                            uint32_t color = (255 << 24) | ((lr & 0xFF) << 16) | ((lg & 0xFF) << 8) | (lb & 0xFF);
                            int dx = abs(x2 - x1), dy = -abs(y2 - y1);
                            int sx = x1 < x2 ? 1 : -1, sy = y1 < y2 ? 1 : -1;
                            int err = dx + dy;
                            while (1) {
                                if (x1 >= 0 && x1 < cw && y1 >= 0 && y1 < ch)
                                    d->pixels[y1 * cw + x1] = color;
                                if (x1 == x2 && y1 == y2) break;
                                int e2 = 2 * err;
                                if (e2 >= dy) { err += dy; x1 += sx; }
                                if (e2 <= dx) { err += dx; y1 += sy; }
                            }
                        } else if (strcmp(cmd, "clear") == 0) {
                            uint32_t color = 0xFFFFFFFF;
                            if (val && val[0]) {
                                int cr, cg, cb;
                                sscanf(val, "%d,%d,%d", &cr, &cg, &cb);
                                color = (255 << 24) | ((cr & 0xFF) << 16) | ((cg & 0xFF) << 8) | (cb & 0xFF);
                            }
                            for (int i = 0; i < cw * ch; i++)
                                d->pixels[i] = color;
                        }
                    }
                }
            }
            fil_result_free(fr);
        }
    }

    out->type = RNODE_CANVAS;
    out->u.canvas.script = arena_strdup(g_session_arena, d->script ? d->script : "");
    out->u.canvas.width = cw;
    out->u.canvas.height = ch;
    out->rect = rect_new(0, 0, cw, ch);
    out->style_class = "canvas";
    out->accessible.role = "canvas";
    out->accessible.label = "Drawing surface";
    out->tab_index = base->tab_index >= 0 ? base->tab_index : -1;
}

static EventResult canvas_handle_event(Widget *self, Event *ev, Backend *backend) {
    (void)self; (void)backend;
    if (ev->type == EVENT_KEY)
        return event_result_response((WidgetResponse){ .result = NULL, .cancelled = false });
    if (ev->type == EVENT_MOUSE_BUTTON || ev->type == EVENT_MOUSE_MOTION)
        return event_result_handled();
    return event_result_unhandled();
}

static void canvas_destroy(Widget *self) {
    CanvasData *d = (CanvasData *)(self + 1);
    free(d->script);
    free(d->pixels);
}

Widget *canvas_widget_new(const char *script, int width, int height) {
    Widget *w = calloc(1, sizeof(Widget) + sizeof(CanvasData));
    CanvasData *d = (CanvasData *)(w + 1);
    d->base.dirty = true;
    d->base.tab_index = -1;
    d->script = script ? strdup(script) : strdup("");
    d->width = width;
    d->height = height;
    d->pixels = NULL;
    d->drawn = false;
    w->vtable.render = canvas_render;
    w->vtable.handle_event = canvas_handle_event;
    w->vtable.is_dirty = widget_base_is_dirty;
    w->vtable.clear_dirty = widget_base_clear_dirty;
    w->vtable.destroy = canvas_destroy;
    return w;
}