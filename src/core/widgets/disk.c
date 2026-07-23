#include "disk.h"
#include "core/widget_base.h"
#include "core/session.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

typedef struct { int number; char *start, *end, *size, *ptype, **flags, *fs_signature; int flag_count; } Partition;
typedef struct { char *start, *end, *size; } FreeSpace;
typedef enum { DMODE_MAIN, DMODE_CONFIRM, DMODE_RESIZE_INPUT, DMODE_NEW_PARTITION } DiskMode;

typedef struct { WidgetBase base; char *title, *disk, *input; Partition *partitions; int part_count; FreeSpace *free_space; int free_count, selected; bool readonly; DiskMode mode; int editing_idx; } DiskData;
extern Arena *g_session_arena;

static long long human_to_bytes(const char *s) {
    char buf[64]; strncpy(buf, s, sizeof(buf)-1); buf[sizeof(buf)-1]='\0';
    char *num_end = buf; while (*num_end && (isdigit(*num_end)||*num_end=='.'||*num_end=='-')) num_end++;
    double num = atof(buf); char *suffix = num_end; while (*suffix==' ') suffix++;
    if (strcasecmp(suffix,"B")==0) return (long long)num;
    if (strcasecmp(suffix,"K")==0||strcasecmp(suffix,"KB")==0||strcasecmp(suffix,"KIB")==0) return (long long)(num*1024);
    if (strcasecmp(suffix,"M")==0||strcasecmp(suffix,"MB")==0||strcasecmp(suffix,"MIB")==0) return (long long)(num*1024*1024);
    if (strcasecmp(suffix,"G")==0||strcasecmp(suffix,"GB")==0||strcasecmp(suffix,"GIB")==0) return (long long)(num*1024*1024*1024);
    if (strcasecmp(suffix,"T")==0||strcasecmp(suffix,"TB")==0||strcasecmp(suffix,"TIB")==0) return (long long)(num*1024*1024*1024*1024);
    return (long long)num;
}

static void disk_render(Widget *self, Rect area, RenderTree *out) {
    DiskData *d = (DiskData *)(self + 1);
    memset(out, 0, sizeof(*out)); out->style_class = "container";
    int box_w = (int)(area.w * 0.95f); if (box_w > area.w - 2) box_w = area.w - 2;
    int box_h = (int)(area.h * 0.95f); if (box_h > area.h - 2) box_h = area.h - 2;
    RenderTree *children = arena_alloc(g_session_arena, 4 * sizeof(RenderTree));
    children[0].type = RNODE_TEXT; children[0].rect = rect_new(1, 1, box_w - 2, 1);
    char title_buf[256]; snprintf(title_buf, sizeof(title_buf), "%s (%s)", d->title, d->disk);
    children[0].text.content = arena_strdup(g_session_arena, title_buf); children[0].style_class = "text"; children[0].state = "title";
    int total = d->part_count + d->free_count;
    children[1].type = RNODE_LIST; children[1].rect = rect_new(1, 2, box_w - 2, box_h - 5);
    children[1].list.item_count = total; children[1].list.selected = d->selected;
    children[1].list.items = arena_alloc(g_session_arena, total * sizeof(ListItem));
    for (int i = 0; i < d->part_count; i++) { char label[256]; snprintf(label, sizeof(label), "%s%3d  %8s  %-22s", i==d->selected?">":" ", d->partitions[i].number, d->partitions[i].size, d->partitions[i].ptype); children[1].list.items[i].label = arena_strdup(g_session_arena, label); }
    for (int i = 0; i < d->free_count; i++) { int sel = d->part_count + i; char label[256]; snprintf(label, sizeof(label), "%s     %8s  Free space", sel==d->selected?">":" ", d->free_space[i].size); children[1].list.items[d->part_count+i].label = arena_strdup(g_session_arena, label); }
    children[1].style_class = "list";
    children[2].type = RNODE_TEXT; children[2].rect = rect_new(1, box_h - 3, box_w - 2, 1);
    if (d->selected < d->part_count && d->mode == DMODE_MAIN) { Partition *p = &d->partitions[d->selected]; char detail[256]; snprintf(detail, sizeof(detail), "Partition %d  Type: %s  Size: %s  FS: %s", p->number, p->ptype, p->size, p->fs_signature?p->fs_signature:"none"); children[2].text.content = arena_strdup(g_session_arena, detail); } else { children[2].text.content = ""; }
    children[2].style_class = "text";
    children[3].type = RNODE_TEXT; children[3].rect = rect_new(1, box_h - 2, box_w - 2, 1);
    children[3].text.content = d->readonly ? "Up/Down:move  Q:quit  Esc:cancel" : "Up/Down:move  N:new  D:delete  R:resize  W:write  Q:quit  Esc:cancel";
    children[3].style_class = "text"; children[3].state = "muted";
    out->type = RNODE_CONTAINER; out->rect = rect_new((area.w - box_w) / 2, (area.h - box_h) / 2, box_w, box_h);
    out->container.border = BORDER_SINGLE; out->container.padding = edgeinsets_zero();
    out->container.children = children; out->container.child_count = 4;
}

static EventResult disk_handle_event(Widget *self, Event *ev, Backend *backend) {
    (void)backend; DiskData *d = (DiskData *)(self + 1);
    if (ev->type != EVENT_KEY) return event_result_unhandled();
    int total = d->part_count + d->free_count;
    if (d->mode == DMODE_CONFIRM) { if (ev->code == KEY_CHAR && (ev->ch=='y'||ev->ch=='Y')) { cJSON *parts = cJSON_CreateArray(); for (int i=0;i<d->part_count;i++) { cJSON *p = cJSON_CreateObject(); cJSON_AddNumberToObject(p,"number",d->partitions[i].number); cJSON_AddStringToObject(p,"start",d->partitions[i].start); cJSON_AddStringToObject(p,"end",d->partitions[i].end); cJSON_AddStringToObject(p,"size",d->partitions[i].size); cJSON_AddStringToObject(p,"type",d->partitions[i].ptype); cJSON_AddItemToArray(parts,p); } return event_result_response((WidgetResponse){ .result = parts, .cancelled = false }); } d->mode = DMODE_MAIN; d->base.dirty = true; return event_result_handled(); }
    if (d->mode == DMODE_RESIZE_INPUT || d->mode == DMODE_NEW_PARTITION) {
        switch (ev->code) {
            case KEY_ESC: free(d->input); d->input = NULL; d->mode = DMODE_MAIN; d->base.dirty = true; return event_result_handled();
            case KEY_ENTER: if (d->input && strlen(d->input)>0) { long long nb = human_to_bytes(d->input); if (nb>0) { if (d->mode==DMODE_NEW_PARTITION) { int fi = d->editing_idx; FreeSpace *fs = &d->free_space[fi]; d->partitions = realloc(d->partitions, (d->part_count+1)*sizeof(Partition)); memset(&d->partitions[d->part_count],0,sizeof(Partition)); d->partitions[d->part_count].number = d->part_count+1; d->partitions[d->part_count].start = strdup(fs->start); d->partitions[d->part_count].size = strdup(d->input); d->partitions[d->part_count].ptype = strdup("Linux filesystem"); d->part_count++; d->selected = d->part_count-1; } else { Partition *p = &d->partitions[d->editing_idx]; free(p->size); p->size = strdup(d->input); } } } free(d->input); d->input = NULL; d->mode = DMODE_MAIN; d->base.dirty = true; return event_result_handled();
            case KEY_BACKSPACE: if (d->input && strlen(d->input)>0) d->input[strlen(d->input)-1]='\0'; d->base.dirty = true; return event_result_handled();
            case KEY_CHAR: { int len = d->input?strlen(d->input):0; d->input = realloc(d->input, len+2); d->input[len]=ev->ch; d->input[len+1]='\0'; d->base.dirty = true; return event_result_handled(); }
            default: return event_result_unhandled();
        }
    }
    switch (ev->code) {
        case KEY_ESC: return event_result_response((WidgetResponse){ .result = NULL, .cancelled = true });
        case KEY_UP: if (d->selected > 0) d->selected--; d->base.dirty = true; return event_result_handled();
        case KEY_DOWN: if (d->selected + 1 < total) d->selected++; d->base.dirty = true; return event_result_handled();
        default: if (ev->code == KEY_CHAR && !d->readonly) { switch (ev->ch) { case 'q': return event_result_response((WidgetResponse){ .result = NULL, .cancelled = true }); case 'n': if (d->selected >= d->part_count) { d->editing_idx = d->selected - d->part_count; d->input = strdup(""); d->mode = DMODE_NEW_PARTITION; d->base.dirty = true; } return event_result_handled(); case 'd': if (d->selected < d->part_count) { free(d->partitions[d->selected].start); free(d->partitions[d->selected].end); free(d->partitions[d->selected].size); free(d->partitions[d->selected].ptype); memmove(&d->partitions[d->selected], &d->partitions[d->selected+1], (d->part_count-d->selected-1)*sizeof(Partition)); d->part_count--; if (d->selected >= d->part_count && d->selected > 0) d->selected--; d->base.dirty = true; } return event_result_handled(); case 'r': if (d->selected < d->part_count) { d->editing_idx = d->selected; d->input = strdup(""); d->mode = DMODE_RESIZE_INPUT; d->base.dirty = true; } return event_result_handled(); case 'w': d->mode = DMODE_CONFIRM; d->base.dirty = true; return event_result_handled(); } } return event_result_unhandled();
    }
}

static void disk_destroy(Widget *self) { DiskData *d = (DiskData *)(self + 1); free(d->title); free(d->disk); free(d->input); for (int i=0;i<d->part_count;i++) { free(d->partitions[i].start); free(d->partitions[i].end); free(d->partitions[i].size); free(d->partitions[i].ptype); } free(d->partitions); for (int i=0;i<d->free_count;i++) { free(d->free_space[i].start); free(d->free_space[i].end); free(d->free_space[i].size); } free(d->free_space); }

Widget *disk_widget_new(const char *title, const char *disk, cJSON *partitions_json, cJSON *free_space_json, bool readonly) {
    Widget *w = calloc(1, sizeof(Widget) + sizeof(DiskData));
    DiskData data = { .title = strdup(title), .disk = strdup(disk), .readonly = readonly, .selected = 0, .mode = DMODE_MAIN };
    if (partitions_json && cJSON_IsArray(partitions_json)) { data.part_count = cJSON_GetArraySize(partitions_json); data.partitions = calloc(data.part_count, sizeof(Partition)); int i=0; cJSON *item; cJSON_ArrayForEach(item, partitions_json) { cJSON *n=cJSON_GetObjectItem(item,"number"); cJSON *s=cJSON_GetObjectItem(item,"start"); cJSON *e=cJSON_GetObjectItem(item,"end"); cJSON *sz=cJSON_GetObjectItem(item,"size"); cJSON *t=cJSON_GetObjectItem(item,"type"); data.partitions[i].number = n?n->valueint:i+1; data.partitions[i].start = s&&s->valuestring?strdup(s->valuestring):strdup(""); data.partitions[i].end = e&&e->valuestring?strdup(e->valuestring):strdup(""); data.partitions[i].size = sz&&sz->valuestring?strdup(sz->valuestring):strdup(""); data.partitions[i].ptype = t&&t->valuestring?strdup(t->valuestring):strdup(""); i++; } }
    if (free_space_json && cJSON_IsArray(free_space_json)) { data.free_count = cJSON_GetArraySize(free_space_json); data.free_space = calloc(data.free_count, sizeof(FreeSpace)); int i=0; cJSON *item; cJSON_ArrayForEach(item, free_space_json) { cJSON *s=cJSON_GetObjectItem(item,"start"); cJSON *e=cJSON_GetObjectItem(item,"end"); cJSON *sz=cJSON_GetObjectItem(item,"size"); data.free_space[i].start = s&&s->valuestring?strdup(s->valuestring):strdup("0"); data.free_space[i].end = e&&e->valuestring?strdup(e->valuestring):strdup("0"); data.free_space[i].size = sz&&sz->valuestring?strdup(sz->valuestring):strdup("0"); i++; } }
    widget_base_init(w, &data, sizeof(DiskData), disk_render, disk_handle_event, disk_destroy);
    return w;
}