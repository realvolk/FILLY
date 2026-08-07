#include "markdown.h"
#include "core/widget_base.h"
#include "core/session.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct {
    WidgetBase base;
    char *content;
} MarkdownData;

extern Arena *g_session_arena;

static void parse_line(RenderTree *children, int *idx, const char *line, int max_w, int ch) {
    if (!line || !line[0]) {
        children[*idx].type = RNODE_TEXT;
        children[*idx].rect = rect_new(0, 0, max_w, ch);
        children[*idx].u.text.content = arena_strdup(g_session_arena, "");
        children[*idx].style_class = "text";
        (*idx)++;
        return;
    }

    if (strncmp(line, "# ", 2) == 0) {
        children[*idx].type = RNODE_TEXT;
        children[*idx].rect = rect_new(0, 0, max_w, ch);
        children[*idx].u.text.content = arena_strdup(g_session_arena, line + 2);
        children[*idx].style_class = "text";
        children[*idx].state = "title";
        (*idx)++;
    } else if (strncmp(line, "## ", 3) == 0) {
        children[*idx].type = RNODE_TEXT;
        children[*idx].rect = rect_new(0, 0, max_w, ch);
        children[*idx].u.text.content = arena_strdup(g_session_arena, line + 3);
        children[*idx].style_class = "text";
        children[*idx].state = "title";
        (*idx)++;
    } else if (strncmp(line, "- ", 2) == 0) {
        char buf[1024];
        snprintf(buf, sizeof(buf), "  • %s", line + 2);
        children[*idx].type = RNODE_TEXT;
        children[*idx].rect = rect_new(0, 0, max_w, ch);
        children[*idx].u.text.content = arena_strdup(g_session_arena, buf);
        children[*idx].style_class = "text";
        (*idx)++;
    } else {
        children[*idx].type = RNODE_TEXT;
        children[*idx].rect = rect_new(0, 0, max_w, ch);
        children[*idx].u.text.content = arena_strdup(g_session_arena, line);
        children[*idx].style_class = "text";
        (*idx)++;
    }
}

static void markdown_render(Widget *self, RenderTree *out) {
    MarkdownData *d = (MarkdownData *)(self + 1);
    WidgetBase *base = (WidgetBase *)(self + 1);
    Rect area = base->render_area;
    memset(out, 0, sizeof(*out));
    out->accessible.role = "markdown";
    out->accessible.label = "Markdown content";
    out->tab_index = base->tab_index >= 0 ? base->tab_index : -1;

    int ch = (area.w > 200) ? 30 : 1;
    int box_w = (int)(area.w * 0.8f);
    if (box_w > area.w - 2) box_w = area.w - 2;

    char *content = d->content ? strdup(d->content) : strdup("");
    int line_count = 1;
    for (char *p = content; *p; p++)
        if (*p == '\n') line_count++;

    RenderTree *children = arena_alloc(g_session_arena, line_count * sizeof(RenderTree));
    int idx = 0;

    char *saveptr;
    char *line = strtok_r(content, "\n", &saveptr);
    while (line) {
        parse_line(children, &idx, line, box_w - 2, ch);
        line = strtok_r(NULL, "\n", &saveptr);
    }

    int box_h = idx * ch + 2;
    if (box_h > area.h) box_h = area.h;

    for (int i = 0; i < idx; i++) {
        children[i].rect.x = 1;
        children[i].rect.y = 1 + i * ch;
        children[i].rect.w = box_w - 2;
        children[i].rect.h = ch;
    }

    out->type = RNODE_CONTAINER;
    out->rect = rect_new((area.w - box_w) / 2, (area.h - box_h) / 2, box_w, box_h);
    out->u.container.border = BORDER_SINGLE;
    out->u.container.padding = edgeinsets_zero();
    out->u.container.children = children;
    out->u.container.child_count = idx;
    out->style_class = "container";

    free(content);
}

static EventResult markdown_handle_event(Widget *self, Event *ev, Backend *backend) {
    (void)self; (void)backend;
    if (ev->type == EVENT_KEY)
        return event_result_response((WidgetResponse){ .result = NULL, .cancelled = false });
    return event_result_unhandled();
}

static void markdown_destroy(Widget *self) {
    MarkdownData *d = (MarkdownData *)(self + 1);
    free(d->content);
}

Widget *markdown_widget_new(const char *content) {
    Widget *w = calloc(1, sizeof(Widget) + sizeof(MarkdownData));
    MarkdownData *d = (MarkdownData *)(w + 1);
    d->base.dirty = true;
    d->base.tab_index = -1;
    d->content = content ? strdup(content) : strdup("");
    w->vtable.render = markdown_render;
    w->vtable.handle_event = markdown_handle_event;
    w->vtable.is_dirty = widget_base_is_dirty;
    w->vtable.clear_dirty = widget_base_clear_dirty;
    w->vtable.destroy = markdown_destroy;
    return w;
}