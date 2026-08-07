#include "image.h"
#include "core/widget_base.h"
#include "core/session.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

typedef struct {
    WidgetBase base;
    char *source;
    char *fit;
    int width;
    int height;
    unsigned char *pixels;
    int pix_w;
    int pix_h;
    bool loaded;
} ImageData;

extern Arena *g_session_arena;

static void image_render(Widget *self, RenderTree *out) {
    ImageData *d = (ImageData *)(self + 1);
    WidgetBase *base = (WidgetBase *)(self + 1);
    Rect area = base->render_area;
    memset(out, 0, sizeof(*out));
    out->accessible.role = "image";
    out->accessible.label = d->source ? d->source : "Image";
    out->tab_index = base->tab_index >= 0 ? base->tab_index : -1;

    if (!d->loaded && d->source && d->source[0]) {
        int w, h, channels;
        unsigned char *pixels = stbi_load(d->source, &w, &h, &channels, 4);
        if (pixels) {
            free(d->pixels);
            d->pixels = pixels;
            d->pix_w = w;
            d->pix_h = h;
            d->loaded = true;
        }
    }

    int display_w = d->width > 0 ? d->width : area.w;
    int display_h = d->height > 0 ? d->height : area.h;

    if (d->loaded && d->pixels && d->pix_w > 0 && d->pix_h > 0) {
        float scale_x = (float)display_w / d->pix_w;
        float scale_y = (float)display_h / d->pix_h;
        float scale;

        if (d->fit && strcmp(d->fit, "cover") == 0)
            scale = scale_x > scale_y ? scale_x : scale_y;
        else if (d->fit && strcmp(d->fit, "fill") == 0) {
            scale_x = (float)display_w / d->pix_w;
            scale_y = (float)display_h / d->pix_h;
            scale = 0;
        } else
            scale = scale_x < scale_y ? scale_x : scale_y;

        int img_w, img_h;
        if (scale > 0) {
            img_w = (int)(d->pix_w * scale);
            img_h = (int)(d->pix_h * scale);
        } else {
            img_w = display_w;
            img_h = display_h;
        }

        out->type = RNODE_IMAGE;
        out->u.image.source = arena_strdup(g_session_arena, d->source);
        out->u.image.fit = arena_strdup(g_session_arena, d->fit ? d->fit : "contain");
        out->u.image.width = img_w;
        out->u.image.height = img_h;
        out->rect = rect_new((area.w - img_w) / 2, (area.h - img_h) / 2, img_w, img_h);
    } else {
        out->type = RNODE_IMAGE;
        out->u.image.source = arena_strdup(g_session_arena, d->source ? d->source : "");
        out->u.image.fit = arena_strdup(g_session_arena, d->fit ? d->fit : "contain");
        out->u.image.width = display_w;
        out->u.image.height = display_h;
        out->rect = rect_new(0, 0, display_w, display_h);
    }
    out->style_class = "image";
}

static EventResult image_handle_event(Widget *self, Event *ev, Backend *backend) {
    (void)self; (void)backend;
    if (ev->type == EVENT_KEY || ev->type == EVENT_MOUSE_BUTTON)
        return event_result_response((WidgetResponse){ .result = NULL, .cancelled = false });
    return event_result_unhandled();
}

static void image_destroy(Widget *self) {
    ImageData *d = (ImageData *)(self + 1);
    free(d->source);
    free(d->fit);
    free(d->pixels);
}

Widget *image_widget_new(const char *source, const char *fit, int width, int height) {
    Widget *w = calloc(1, sizeof(Widget) + sizeof(ImageData));
    ImageData *d = (ImageData *)(w + 1);
    d->base.dirty = true;
    d->base.tab_index = -1;
    d->source = source ? strdup(source) : strdup("");
    d->fit = fit ? strdup(fit) : strdup("contain");
    d->width = width;
    d->height = height;
    d->pixels = NULL;
    d->pix_w = 0;
    d->pix_h = 0;
    d->loaded = false;
    w->vtable.render = image_render;
    w->vtable.handle_event = image_handle_event;
    w->vtable.is_dirty = widget_base_is_dirty;
    w->vtable.clear_dirty = widget_base_clear_dirty;
    w->vtable.destroy = image_destroy;
    return w;
}