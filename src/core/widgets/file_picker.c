#include "file_picker.h"
#include "core/widget_base.h"
#include "core/session.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>

typedef struct { WidgetBase base; char *title, *current_dir, *filter_ext, **entries; int entry_count, selected; bool *is_dir; } FilePickerData;
extern Arena *g_session_arena;

static void fp_populate(FilePickerData *d) {
    for (int i = 0; i < d->entry_count; i++) free(d->entries[i]);
    free(d->entries);
    free(d->is_dir);
    d->entries = NULL;
    d->is_dir = NULL;
    d->entry_count = 0;
    DIR *dir = opendir(d->current_dir);
    if (!dir) return;
    int cap = 64;
    d->entries = malloc(cap * sizeof(char *));
    d->is_dir = malloc(cap * sizeof(bool));
    struct dirent *entry;
    while ((entry = readdir(dir))) {
        if (strcmp(entry->d_name, ".") == 0) continue;
        char path[4096];
        snprintf(path, sizeof(path), "%s/%s", d->current_dir, entry->d_name);
        struct stat st;
        if (stat(path, &st) != 0) continue;
        bool isdir = S_ISDIR(st.st_mode);
        if (!isdir && d->filter_ext && strlen(d->filter_ext)) {
            const char *ext = strrchr(entry->d_name, '.');
            if (!ext || strcmp(ext + 1, d->filter_ext) != 0) continue;
        }
        if (d->entry_count >= cap) {
            cap *= 2;
            d->entries = realloc(d->entries, cap * sizeof(char *));
            d->is_dir = realloc(d->is_dir, cap * sizeof(bool));
        }
        d->entries[d->entry_count] = strdup(entry->d_name);
        d->is_dir[d->entry_count] = isdir;
        d->entry_count++;
    }
    closedir(dir);
    if (d->selected >= d->entry_count) d->selected = d->entry_count > 0 ? d->entry_count - 1 : 0;
}

static void file_picker_render(Widget *self, Rect area, RenderTree *out) {
    FilePickerData *d = (FilePickerData *)(self + 1);
    memset(out, 0, sizeof(*out));
    out->style_class = "container";
    int box_w = (int)(area.w * 0.7f);
    if (box_w > area.w - 2) box_w = area.w - 2;
    int box_h = (int)(area.h * 0.75f);
    if (box_h > area.h - 2) box_h = area.h - 2;
    RenderTree *children = arena_alloc(g_session_arena, 3 * sizeof(RenderTree));
    children[0].type = RNODE_TEXT;
    children[0].rect = rect_new(1, 0, box_w - 2, 1);
    char title_buf[512];
    snprintf(title_buf, sizeof(title_buf), "%s: %s", d->title, d->current_dir);
    children[0].text.content = arena_strdup(g_session_arena, title_buf);
    children[0].style_class = "text";
    children[0].state = "title";
    children[1].type = RNODE_LIST;
    children[1].rect = rect_new(1, 1, box_w - 2, box_h - 3);
    children[1].list.item_count = d->entry_count;
    children[1].list.selected = d->selected;
    children[1].list.items = arena_alloc(g_session_arena, d->entry_count * sizeof(ListItem));
    for (int i = 0; i < d->entry_count; i++) {
        char label[512];
        snprintf(label, sizeof(label), "%s %s", d->is_dir[i] ? "[DIR]" : "     ", d->entries[i]);
        children[1].list.items[i].label = arena_strdup(g_session_arena, label);
    }
    children[1].style_class = "list";
    children[2].type = RNODE_TEXT;
    children[2].rect = rect_new(1, box_h - 2, box_w - 2, 1);
    children[2].text.content = "Up/Down:move  Enter:open/select  Backspace:parent  Esc:cancel";
    children[2].style_class = "text";
    children[2].state = "muted";
    out->type = RNODE_CONTAINER;
    out->rect = rect_new((area.w - box_w) / 2, (area.h - box_h) / 2, box_w, box_h);
    out->container.border = BORDER_SINGLE;
    out->container.padding = edgeinsets_zero();
    out->container.children = children;
    out->container.child_count = 3;
}

static EventResult file_picker_handle_event(Widget *self, Event *ev, Backend *backend) {
    (void)backend;
    FilePickerData *d = (FilePickerData *)(self + 1);
    if (ev->type != EVENT_KEY) return event_result_unhandled();
    switch (ev->code) {
        case KEY_ESC:
            return event_result_response((WidgetResponse){ .result = NULL, .cancelled = true });
        case KEY_UP:
            if (d->selected > 0) { d->selected--; d->base.dirty = true; }
            return event_result_handled();
        case KEY_DOWN:
            if (d->selected + 1 < d->entry_count) { d->selected++; d->base.dirty = true; }
            return event_result_handled();
        case KEY_ENTER:
            if (d->selected < d->entry_count) {
                if (d->is_dir[d->selected]) {
                    char *np = malloc(strlen(d->current_dir) + strlen(d->entries[d->selected]) + 2);
                    sprintf(np, "%s/%s", d->current_dir, d->entries[d->selected]);
                    free(d->current_dir);
                    d->current_dir = np;
                    fp_populate(d);
                    d->base.dirty = true;
                } else {
                    char *full = malloc(strlen(d->current_dir) + strlen(d->entries[d->selected]) + 2);
                    sprintf(full, "%s/%s", d->current_dir, d->entries[d->selected]);
                    WidgetResponse r = { .result = cJSON_CreateString(full), .cancelled = false };
                    free(full);
                    return event_result_response(r);
                }
            }
            return event_result_handled();
        case KEY_BACKSPACE: {
            char *slash = strrchr(d->current_dir, '/');
            if (slash && slash != d->current_dir) {
                *slash = '\0';
                fp_populate(d);
                d->base.dirty = true;
            }
            return event_result_handled();
        }
        default:
            return event_result_unhandled();
    }
}

static void fp_destroy(Widget *self) {
    FilePickerData *d = (FilePickerData *)(self + 1);
    free(d->title);
    free(d->current_dir);
    free(d->filter_ext);
    for (int i = 0; i < d->entry_count; i++) free(d->entries[i]);
    free(d->entries);
    free(d->is_dir);
}

Widget *file_picker_widget_new(const char *title, const char *start_dir, const char *filter_ext) {
    Widget *w = calloc(1, sizeof(Widget) + sizeof(FilePickerData));
    FilePickerData data = {
        .title = strdup(title),
        .current_dir = start_dir ? strdup(start_dir) : strdup("/"),
        .filter_ext = filter_ext ? strdup(filter_ext) : NULL,
        .entry_count = 0,
        .selected = 0
    };
    fp_populate(&data);
    widget_base_init(w, &data, sizeof(FilePickerData), file_picker_render, file_picker_handle_event, fp_destroy);
    return w;
}