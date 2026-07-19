#include <stdio.h>
#include "file_picker.h"
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

typedef struct {
    char *title;
    char *current_dir;
    char *filter_ext;
    char **entries;
    int entry_count;
    int selected;
    bool *is_dir;
    bool dirty;
} FilePickerData;

static void fp_populate(FilePickerData *d) {
    for (int i = 0; i < d->entry_count; i++) free(d->entries[i]);
    free(d->entries); free(d->is_dir);
    d->entries = NULL; d->is_dir = NULL; d->entry_count = 0;

    DIR *dir = opendir(d->current_dir);
    if (!dir) return;
    struct dirent *entry;
    int cap = 64;
    d->entries = malloc(cap * sizeof(char *));
    d->is_dir = malloc(cap * sizeof(bool));
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
        if (d->entry_count >= cap) { cap *= 2; d->entries = realloc(d->entries, cap * sizeof(char *)); d->is_dir = realloc(d->is_dir, cap * sizeof(bool)); }
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
    int box_w = (int)(area.w * 0.7f);
    if (box_w > area.w - 2) box_w = area.w - 2;
    int box_h = (int)(area.h * 0.75f);
    if (box_h > area.h - 2) box_h = area.h - 2;
    int box_x = (area.w - box_w) / 2, box_y = (area.h - box_h) / 2;

    out->accessible.role = strdup("dialog");
    out->accessible.label = strdup(d->title ? d->title : "File Picker");

    RenderTree *children = calloc(3 + 1, sizeof(RenderTree));
    children[0].type = RNODE_TEXT;
    children[0].rect = rect_new(1, 0, box_w - 2, 1);
    char title_buf[512];
    snprintf(title_buf, sizeof(title_buf), "%s: %s", d->title, d->current_dir);
    children[0].text.content = strdup(title_buf);
    children[0].text.align = ALIGN_CENTER;
    children[0].text.style = textstyle_selected();
    children[0].accessible.role = strdup("heading");
    children[0].accessible.label = strdup(title_buf);

    children[1].type = RNODE_LIST;
    children[1].rect = rect_new(1, 1, box_w - 2, box_h - 3);
    children[1].list.item_count = d->entry_count;
    children[1].list.items = malloc(d->entry_count * sizeof(ListItem));
    for (int i = 0; i < d->entry_count; i++) {
        char label[512];
        snprintf(label, sizeof(label), "%s %s", d->is_dir[i] ? "[DIR]" : "     ", d->entries[i]);
        children[1].list.items[i] = listitem_new(label);
    }
    children[1].list.selected = d->selected;
    children[1].list.highlight = textstyle_selected();
    children[1].accessible.role = strdup("listbox");

    children[2].type = RNODE_TEXT;
    children[2].rect = rect_new(1, box_h - 2, box_w - 2, 1);
    children[2].text.content = strdup("Up/Down:move  Enter:open/select  Backspace:parent  Esc:cancel");
    children[2].text.align = ALIGN_CENTER;
    children[2].text.style = textstyle_muted();

    out->type = RNODE_CONTAINER;
    out->rect = rect_new(box_x, box_y, box_w, box_h);
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
            return event_result_response((WidgetResponse){ .result = NULL, .cancelled = true, .error = NULL });
        case KEY_UP:
            if (d->selected > 0) { d->selected--; d->dirty = true; }
            return event_result_handled();
        case KEY_DOWN:
            if (d->selected + 1 < d->entry_count) { d->selected++; d->dirty = true; }
            return event_result_handled();
        case KEY_ENTER:
            if (d->selected < d->entry_count) {
                if (d->is_dir[d->selected]) {
                    char *new_path = malloc(strlen(d->current_dir) + strlen(d->entries[d->selected]) + 2);
                    sprintf(new_path, "%s/%s", d->current_dir, d->entries[d->selected]);
                    free(d->current_dir);
                    d->current_dir = new_path;
                    fp_populate(d);
                    d->dirty = true;
                } else {
                    char *full = malloc(strlen(d->current_dir) + strlen(d->entries[d->selected]) + 2);
                    sprintf(full, "%s/%s", d->current_dir, d->entries[d->selected]);
                    return event_result_response((WidgetResponse){ .result = cJSON_CreateString(full), .cancelled = false, .error = NULL });
                }
            }
            return event_result_handled();
        case KEY_BACKSPACE: {
            char *slash = strrchr(d->current_dir, '/');
            if (slash && slash != d->current_dir) {
                *slash = '\0';
                fp_populate(d);
                d->dirty = true;
            }
            return event_result_handled();
        }
        default: return event_result_unhandled();
    }
}

static bool fp_is_dirty(Widget *self) { return ((FilePickerData *)(self + 1))->dirty; }
static void fp_clear_dirty(Widget *self) { ((FilePickerData *)(self + 1))->dirty = false; }
static void fp_destroy(Widget *self) {
    FilePickerData *d = (FilePickerData *)(self + 1);
    free(d->title); free(d->current_dir); free(d->filter_ext);
    for (int i = 0; i < d->entry_count; i++) free(d->entries[i]);
    free(d->entries); free(d->is_dir);
}

Widget *file_picker_widget_new(const char *title, const char *start_dir, const char *filter_ext) {
    Widget *w = calloc(1, sizeof(Widget) + sizeof(FilePickerData));
    w->vtable.render = file_picker_render;
    w->vtable.handle_event = file_picker_handle_event;
    w->vtable.is_dirty = fp_is_dirty;
    w->vtable.clear_dirty = fp_clear_dirty;
    w->vtable.destroy = fp_destroy;
    FilePickerData *d = (FilePickerData *)(w + 1);
    d->title = strdup(title);
    d->current_dir = start_dir ? strdup(start_dir) : strdup("/");
    d->filter_ext = filter_ext ? strdup(filter_ext) : NULL;
    d->entries = NULL; d->entry_count = 0; d->is_dir = NULL; d->selected = 0;
    fp_populate(d);
    d->dirty = true;
    return w;
}