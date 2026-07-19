#include "disk.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

typedef struct {
    int number;
    char *start;
    char *end;
    char *size;
    char *ptype;
    char **flags;
    int flag_count;
    char *fs_signature;
} Partition;

typedef struct {
    char *start;
    char *end;
    char *size;
} FreeSpace;

typedef enum {
    DMODE_MAIN,
    DMODE_TYPE_PICKER,
    DMODE_FLAG_PICKER,
    DMODE_RESIZE_INPUT,
    DMODE_NEW_PARTITION,
    DMODE_CONFIRM
} DiskMode;

typedef struct {
    char *title;
    char *disk;
    Partition *partitions;
    int part_count;
    FreeSpace *free_space;
    int free_count;
    bool readonly;
    int selected;
    DiskMode mode;
    int type_selected;
    int flag_selected;
    int editing_idx;
    char *input;
    char *confirm_title;
    char *confirm_msg;
    int confirm_action;
    bool dirty;
} DiskData;

static int str_case_eq(const char *a, const char *b) {
    while (*a && *b) {
        char ca = (*a >= 'A' && *a <= 'Z') ? *a + 32 : *a;
        char cb = (*b >= 'A' && *b <= 'Z') ? *b + 32 : *b;
        if (ca != cb) return 0;
        a++; b++;
    }
    return *a == *b;
}

static long long human_to_bytes(const char *s) {
    char buf[64];
    strncpy(buf, s, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    char *num_end = buf;
    while (*num_end && (isdigit(*num_end) || *num_end == '.' || *num_end == '-')) num_end++;
    double num = atof(buf);
    char *suffix = num_end;
    while (*suffix == ' ') suffix++;
    if (str_case_eq(suffix, "B")) return (long long)num;
    if (str_case_eq(suffix, "K") || str_case_eq(suffix, "KB") || str_case_eq(suffix, "KIB"))
        return (long long)(num * 1024);
    if (str_case_eq(suffix, "M") || str_case_eq(suffix, "MB") || str_case_eq(suffix, "MIB"))
        return (long long)(num * 1024 * 1024);
    if (str_case_eq(suffix, "G") || str_case_eq(suffix, "GB") || str_case_eq(suffix, "GIB"))
        return (long long)(num * 1024 * 1024 * 1024);
    if (str_case_eq(suffix, "T") || str_case_eq(suffix, "TB") || str_case_eq(suffix, "TIB"))
        return (long long)(num * 1024 * 1024 * 1024 * 1024);
    return (long long)num;
}

static const char *partition_type_choices[] = {
    "EFI System", "BIOS boot", "Linux filesystem", "Linux swap",
    "Linux LVM", "Linux LUKS", "Linux RAID", "Linux /boot",
    "Linux /home", "Linux /var", "Linux /tmp", "Windows data",
    "Windows recovery", "FreeBSD", "FreeBSD swap", "FreeBSD ZFS",
    "FreeBSD UFS", "macOS APFS", "macOS HFS+", "Solaris", "Custom"
};
static int pt_count = 21;

static const char *flag_choices[] = {"boot", "esp", "bios_grub", "lvm", "raid"};
static int flag_count = 5;

static Partition parse_partition(cJSON *v) {
    Partition p = {0};
    cJSON *n = cJSON_GetObjectItem(v, "number"); if (n) p.number = n->valueint;
    cJSON *s = cJSON_GetObjectItem(v, "start"); if (s && s->valuestring) p.start = strdup(s->valuestring);
    cJSON *e = cJSON_GetObjectItem(v, "end"); if (e && e->valuestring) p.end = strdup(e->valuestring);
    cJSON *sz = cJSON_GetObjectItem(v, "size"); if (sz && sz->valuestring) p.size = strdup(sz->valuestring);
    cJSON *t = cJSON_GetObjectItem(v, "type"); if (t && t->valuestring) p.ptype = strdup(t->valuestring);
    cJSON *f = cJSON_GetObjectItem(v, "flags");
    if (f && f->type == cJSON_Array) {
        p.flags = malloc(cJSON_GetArraySize(f) * sizeof(char *));
        int i = 0; cJSON *item;
        cJSON_ArrayForEach(item, f) if (item->valuestring) p.flags[i++] = strdup(item->valuestring);
        p.flag_count = i;
    }
    cJSON *fs = cJSON_GetObjectItem(v, "fs_signature"); if (fs && fs->valuestring) p.fs_signature = strdup(fs->valuestring);
    return p;
}

static void disk_render(Widget *self, Rect area, RenderTree *out) {
    DiskData *d = (DiskData *)(self + 1);
    memset(out, 0, sizeof(*out));
    int box_w = (int)(area.w * 0.95f); if (box_w > area.w - 2) box_w = area.w - 2;
    int box_h = (int)(area.h * 0.95f); if (box_h > area.h - 2) box_h = area.h - 2;
    int box_x = (area.w - box_w) / 2, box_y = (area.h - box_h) / 2;

    out->accessible.role = strdup("dialog");
    out->accessible.label = strdup(d->title ? d->title : "Disk");

    RenderTree *children = calloc(5 + 1, sizeof(RenderTree));
    int idx = 0;
    children[idx].type = RNODE_TEXT; children[idx].rect = rect_new(1, 1, box_w - 2, 1);
    char title_buf[256]; snprintf(title_buf, sizeof(title_buf), "%s (%s)", d->title, d->disk);
    children[idx].text.content = strdup(title_buf); children[idx].text.align = ALIGN_CENTER;
    children[idx].text.style = textstyle_selected();
    children[idx].accessible.role = strdup("heading"); children[idx].accessible.label = strdup(title_buf);
    idx++;

    int total = d->part_count + d->free_count;
    children[idx].type = RNODE_LIST; children[idx].rect = rect_new(1, 2, box_w - 2, box_h - 5);
    children[idx].list.item_count = total; children[idx].list.items = malloc(total * sizeof(ListItem));
    for (int i = 0; i < d->part_count; i++) {
        char label[256]; snprintf(label, sizeof(label), "%s%3d  %8s  %-22s", i == d->selected ? ">" : " ", d->partitions[i].number, d->partitions[i].size, d->partitions[i].ptype);
        children[idx].list.items[i] = listitem_new(label);
    }
    for (int i = 0; i < d->free_count; i++) {
        char label[256]; int sel = d->part_count + i;
        snprintf(label, sizeof(label), "%s     %8s  Free space", sel == d->selected ? ">" : " ", d->free_space[i].size);
        children[idx].list.items[d->part_count + i] = listitem_new(label);
    }
    children[idx].list.selected = d->selected; children[idx].list.highlight = textstyle_selected();
    children[idx].accessible.role = strdup("listbox");
    idx++;

    if (d->selected < d->part_count && d->mode == DMODE_MAIN) {
        Partition *p = &d->partitions[d->selected];
        children[idx].type = RNODE_TEXT; children[idx].rect = rect_new(1, box_h - 3, box_w - 2, 1);
        char detail[256]; snprintf(detail, sizeof(detail), "Partition %d  Type: %s  Size: %s  FS: %s", p->number, p->ptype, p->size, p->fs_signature ? p->fs_signature : "none");
        children[idx].text.content = strdup(detail); children[idx].text.align = ALIGN_LEFT;
        children[idx].text.style = textstyle_normal(); idx++;
    }
    children[idx].type = RNODE_TEXT; children[idx].rect = rect_new(1, box_h - 2, box_w - 2, 1);
    char *footer = d->readonly ? "Up/Down:move  Q:quit  Esc:cancel" : "Up/Down:move  N:new  D:delete  R:resize  T:type  F:flags  W:write  Q:quit  Esc:cancel";
    children[idx].text.content = strdup(footer); children[idx].text.align = ALIGN_CENTER;
    children[idx].text.style = textstyle_muted(); idx++;

    out->type = RNODE_CONTAINER;
    out->rect = rect_new(box_x, box_y, box_w, box_h);
    out->container.border = BORDER_SINGLE; out->container.padding = edgeinsets_zero();
    out->container.children = children; out->container.child_count = idx;
}

static EventResult disk_handle_event(Widget *self, Event *ev, Backend *backend) {
    (void)backend;
    DiskData *d = (DiskData *)(self + 1);
    if (ev->type != EVENT_KEY) return event_result_unhandled();
    int total = d->part_count + d->free_count;

    if (d->mode == DMODE_CONFIRM) {
        switch (ev->code) {
            case KEY_CHAR:
                if (ev->ch == 'y' || ev->ch == 'Y') {
                    cJSON *parts = cJSON_CreateArray();
                    for (int i = 0; i < d->part_count; i++) {
                        cJSON *p = cJSON_CreateObject();
                        cJSON_AddNumberToObject(p, "number", d->partitions[i].number);
                        cJSON_AddStringToObject(p, "start", d->partitions[i].start);
                        cJSON_AddStringToObject(p, "end", d->partitions[i].end);
                        cJSON_AddStringToObject(p, "size", d->partitions[i].size);
                        cJSON_AddStringToObject(p, "type", d->partitions[i].ptype);
                        cJSON *fl = cJSON_CreateArray();
                        for (int j = 0; j < d->partitions[i].flag_count; j++) cJSON_AddItemToArray(fl, cJSON_CreateString(d->partitions[i].flags[j]));
                        cJSON_AddItemToObject(p, "flags", fl); cJSON_AddItemToArray(parts, p);
                    }
                    cJSON *free = cJSON_CreateArray();
                    for (int i = 0; i < d->free_count; i++) { cJSON *fs = cJSON_CreateObject(); cJSON_AddStringToObject(fs, "start", d->free_space[i].start); cJSON_AddStringToObject(fs, "end", d->free_space[i].end); cJSON_AddStringToObject(fs, "size", d->free_space[i].size); cJSON_AddItemToArray(free, fs); }
                    cJSON *result = cJSON_CreateObject(); cJSON_AddItemToObject(result, "partitions", parts); cJSON_AddItemToObject(result, "free_space", free);
                    return event_result_response((WidgetResponse){ .result = result, .cancelled = false, .error = NULL });
                }
                d->mode = DMODE_MAIN; d->dirty = true; return event_result_handled();
            default:
                d->mode = DMODE_MAIN; d->dirty = true; return event_result_handled();
        }
    }

    if (d->mode == DMODE_RESIZE_INPUT || d->mode == DMODE_NEW_PARTITION) {
        switch (ev->code) {
            case KEY_ESC: free(d->input); d->input = NULL; d->mode = DMODE_MAIN; d->dirty = true; return event_result_handled();
            case KEY_ENTER:
                if (d->input && strlen(d->input) > 0) {
                    long long new_bytes = human_to_bytes(d->input);
                    if (new_bytes > 0) {
                        if (d->mode == DMODE_NEW_PARTITION) {
                            int fi = d->editing_idx; FreeSpace *fs = &d->free_space[fi];
                            long long fs_start = human_to_bytes(fs->start), fs_end = human_to_bytes(fs->end);
                            long long clamped = new_bytes < (fs_end - fs_start) ? new_bytes : (fs_end - fs_start);
                            d->partitions = realloc(d->partitions, (d->part_count + 1) * sizeof(Partition));
                            d->partitions[d->part_count].number = d->part_count > 0 ? d->partitions[d->part_count - 1].number + 1 : 1;
                            char buf[64]; snprintf(buf, sizeof(buf), "%lld", fs_start); d->partitions[d->part_count].start = strdup(buf);
                            snprintf(buf, sizeof(buf), "%lld", fs_start + clamped); d->partitions[d->part_count].end = strdup(buf);
                            snprintf(buf, sizeof(buf), "%lld", clamped); d->partitions[d->part_count].size = strdup(buf);
                            d->partitions[d->part_count].ptype = strdup("Linux filesystem"); d->partitions[d->part_count].flags = NULL; d->partitions[d->part_count].flag_count = 0; d->partitions[d->part_count].fs_signature = NULL;
                            d->part_count++; long long rem = fs_end - fs_start - clamped;
                            if (rem > 0) { snprintf(buf, sizeof(buf), "%lld", fs_start + clamped); free(d->free_space[fi].start); d->free_space[fi].start = strdup(buf); snprintf(buf, sizeof(buf), "%lld", rem); free(d->free_space[fi].size); d->free_space[fi].size = strdup(buf); }
                            else { free(d->free_space[fi].start); free(d->free_space[fi].end); free(d->free_space[fi].size); memmove(&d->free_space[fi], &d->free_space[fi + 1], (d->free_count - fi - 1) * sizeof(FreeSpace)); d->free_count--; }
                            d->selected = d->part_count - 1;
                        } else {
                            Partition *p = &d->partitions[d->editing_idx];
                            long long old_start = human_to_bytes(p->start), old_end = human_to_bytes(p->end), old_size = old_end - old_start;
                            if (new_bytes < old_size) {
                                long long new_end = old_start + new_bytes; char buf[64];
                                snprintf(buf, sizeof(buf), "%lld", new_end); free(p->end); p->end = strdup(buf);
                                snprintf(buf, sizeof(buf), "%lld", new_bytes); free(p->size); p->size = strdup(buf);
                                d->free_space = realloc(d->free_space, (d->free_count + 1) * sizeof(FreeSpace));
                                snprintf(buf, sizeof(buf), "%lld", new_end); d->free_space[d->free_count].start = strdup(buf);
                                snprintf(buf, sizeof(buf), "%lld", old_end); d->free_space[d->free_count].end = strdup(buf);
                                snprintf(buf, sizeof(buf), "%lld", old_end - new_end); d->free_space[d->free_count].size = strdup(buf); d->free_count++;
                            }
                        }
                    }
                }
                free(d->input); d->input = NULL; d->mode = DMODE_MAIN; d->dirty = true; return event_result_handled();
            case KEY_BACKSPACE: if (d->input && strlen(d->input) > 0) d->input[strlen(d->input) - 1] = '\0'; d->dirty = true; return event_result_handled();
            case KEY_CHAR: { int len = d->input ? strlen(d->input) : 0; d->input = realloc(d->input, len + 2); d->input[len] = ev->ch; d->input[len + 1] = '\0'; d->dirty = true; return event_result_handled(); }
            default: return event_result_unhandled();
        }
    }

    switch (ev->code) {
        case KEY_ESC: return event_result_response((WidgetResponse){ .result = NULL, .cancelled = true, .error = NULL });
        case KEY_UP: if (d->selected > 0) d->selected--; d->dirty = true; return event_result_handled();
        case KEY_DOWN: if (d->selected + 1 < total) d->selected++; d->dirty = true; return event_result_handled();
        default: break;
    }

    if (ev->code == KEY_CHAR && !d->readonly) {
        switch (ev->ch) {
            case 'q': return event_result_response((WidgetResponse){ .result = NULL, .cancelled = true, .error = NULL });
            case 'n': if (d->selected >= d->part_count && d->selected - d->part_count < d->free_count) { d->editing_idx = d->selected - d->part_count; d->input = strdup(""); d->mode = DMODE_NEW_PARTITION; d->dirty = true; } return event_result_handled();
            case 'd': if (d->selected < d->part_count) { Partition *p = &d->partitions[d->selected]; d->free_space = realloc(d->free_space, (d->free_count + 1) * sizeof(FreeSpace)); d->free_space[d->free_count].start = strdup(p->start); d->free_space[d->free_count].end = strdup(p->end); d->free_space[d->free_count].size = strdup(p->size); d->free_count++; free(p->start); free(p->end); free(p->size); free(p->ptype); for (int i = 0; i < p->flag_count; i++) free(p->flags[i]); free(p->flags); free(p->fs_signature); memmove(&d->partitions[d->selected], &d->partitions[d->selected + 1], (d->part_count - d->selected - 1) * sizeof(Partition)); d->part_count--; if (d->selected >= d->part_count && d->selected > 0) d->selected--; d->dirty = true; } return event_result_handled();
            case 't': if (d->selected < d->part_count) { d->editing_idx = d->selected; d->type_selected = 0; d->mode = DMODE_TYPE_PICKER; d->dirty = true; } return event_result_handled();
            case 'f': if (d->selected < d->part_count) { d->editing_idx = d->selected; d->flag_selected = 0; d->mode = DMODE_FLAG_PICKER; d->dirty = true; } return event_result_handled();
            case 'r': if (d->selected < d->part_count) { d->editing_idx = d->selected; d->input = strdup(""); d->mode = DMODE_RESIZE_INPUT; d->dirty = true; } return event_result_handled();
            case 'w': d->mode = DMODE_CONFIRM; d->confirm_title = "Write Changes"; d->dirty = true; return event_result_handled();
        }
    }
    return event_result_unhandled();
}

static bool disk_is_dirty(Widget *self) { return ((DiskData *)(self + 1))->dirty; }
static void disk_clear_dirty(Widget *self) { ((DiskData *)(self + 1))->dirty = false; }
static void disk_destroy(Widget *self) {
    DiskData *d = (DiskData *)(self + 1);
    free(d->title); free(d->disk); free(d->input); free(d->confirm_msg);
    for (int i = 0; i < d->part_count; i++) { free(d->partitions[i].start); free(d->partitions[i].end); free(d->partitions[i].size); free(d->partitions[i].ptype); for (int j = 0; j < d->partitions[i].flag_count; j++) free(d->partitions[i].flags[j]); free(d->partitions[i].flags); free(d->partitions[i].fs_signature); }
    free(d->partitions);
    for (int i = 0; i < d->free_count; i++) { free(d->free_space[i].start); free(d->free_space[i].end); free(d->free_space[i].size); }
    free(d->free_space);
}

Widget *disk_widget_new(const char *title, const char *disk, cJSON *partitions_json, cJSON *free_space_json, bool readonly) {
    Widget *w = calloc(1, sizeof(Widget) + sizeof(DiskData));
    w->vtable.render = disk_render; w->vtable.handle_event = disk_handle_event;
    w->vtable.is_dirty = disk_is_dirty; w->vtable.clear_dirty = disk_clear_dirty; w->vtable.destroy = disk_destroy;
    DiskData *d = (DiskData *)(w + 1);
    d->title = strdup(title); d->disk = strdup(disk); d->readonly = readonly;
    d->selected = 0; d->mode = DMODE_MAIN; d->type_selected = 0; d->flag_selected = 0; d->editing_idx = 0;
    d->input = NULL; d->confirm_title = NULL; d->confirm_msg = NULL; d->confirm_action = 0; d->dirty = true;
    d->part_count = 0; d->partitions = NULL;
    if (partitions_json && partitions_json->type == cJSON_Array) { d->part_count = cJSON_GetArraySize(partitions_json); d->partitions = malloc(d->part_count * sizeof(Partition)); int i = 0; cJSON *item; cJSON_ArrayForEach(item, partitions_json) d->partitions[i++] = parse_partition(item); }
    d->free_count = 0; d->free_space = NULL;
    if (free_space_json && free_space_json->type == cJSON_Array) { d->free_count = cJSON_GetArraySize(free_space_json); d->free_space = malloc(d->free_count * sizeof(FreeSpace)); int i = 0; cJSON *item; cJSON_ArrayForEach(item, free_space_json) { cJSON *s = cJSON_GetObjectItem(item, "start"); cJSON *e = cJSON_GetObjectItem(item, "end"); cJSON *sz = cJSON_GetObjectItem(item, "size"); d->free_space[i].start = s && s->valuestring ? strdup(s->valuestring) : strdup("0"); d->free_space[i].end = e && e->valuestring ? strdup(e->valuestring) : strdup("0"); d->free_space[i].size = sz && sz->valuestring ? strdup(sz->valuestring) : strdup("0"); i++; } }
    (void)partition_type_choices; (void)pt_count; (void)flag_choices; (void)flag_count;
    return w;
}