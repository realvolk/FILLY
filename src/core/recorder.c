#include "recorder.h"
#include "cJSON.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#define MAX_SNAPSHOTS 1024
#define MAX_EVENTS 65536

typedef struct {
    char *widget_type;
    char *json_params;
    WidgetResponse response;
    long long timestamp_ms;
    char *ansi_buffer;
    int ansi_len;
} RecordedFrame;

typedef struct {
    RecordedFrame frames[MAX_SNAPSHOTS];
    int frame_count;
    Event events[MAX_EVENTS];
    int event_count;
    bool recording;
    bool replaying;
    int replay_pos;
    long long start_time;
} Recorder;

static Recorder g_recorder = {0};

void recorder_start(void) {
    recorder_reset();
    g_recorder.recording = true;
    g_recorder.start_time = time(NULL) * 1000;
}

void recorder_stop(void) {
    g_recorder.recording = false;
}

void recorder_record_frame(const char *widget_type, const char *json_params, WidgetResponse resp, const char *ansi_buf, int ansi_len) {
    if (!g_recorder.recording) return;
    if (g_recorder.frame_count >= MAX_SNAPSHOTS) return;

    RecordedFrame *f = &g_recorder.frames[g_recorder.frame_count++];
    f->widget_type = widget_type ? strdup(widget_type) : NULL;
    f->json_params = json_params ? strdup(json_params) : NULL;
    f->response = resp;
    if (resp.result) f->response.result = cJSON_Duplicate(resp.result, 1);
    f->timestamp_ms = (time(NULL) * 1000) - g_recorder.start_time;
    if (ansi_buf && ansi_len > 0) {
        f->ansi_buffer = malloc(ansi_len + 1);
        memcpy(f->ansi_buffer, ansi_buf, ansi_len);
        f->ansi_buffer[ansi_len] = '\0';
        f->ansi_len = ansi_len;
    } else {
        f->ansi_buffer = NULL;
        f->ansi_len = 0;
    }
}

void recorder_record_event(Event ev) {
    if (!g_recorder.recording) return;
    if (g_recorder.event_count >= MAX_EVENTS) return;
    g_recorder.events[g_recorder.event_count++] = ev;
}

bool recorder_save(const char *path) {
    if (g_recorder.frame_count == 0) return false;

    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "version", 1);
    cJSON_AddNumberToObject(root, "frame_count", g_recorder.frame_count);
    cJSON_AddNumberToObject(root, "event_count", g_recorder.event_count);

    cJSON *frames = cJSON_CreateArray();
    for (int i = 0; i < g_recorder.frame_count; i++) {
        cJSON *f = cJSON_CreateObject();
        cJSON_AddStringToObject(f, "widget_type", g_recorder.frames[i].widget_type);
        cJSON_AddStringToObject(f, "params", g_recorder.frames[i].json_params);
        cJSON_AddNumberToObject(f, "timestamp", g_recorder.frames[i].timestamp_ms);
        if (g_recorder.frames[i].response.result) {
            char *rj = cJSON_PrintUnformatted(g_recorder.frames[i].response.result);
            cJSON_AddStringToObject(f, "response", rj);
            free(rj);
        }
        cJSON_AddBoolToObject(f, "cancelled", g_recorder.frames[i].response.cancelled);
        cJSON_AddItemToArray(frames, f);
    }
    cJSON_AddItemToObject(root, "frames", frames);

    cJSON *events = cJSON_CreateArray();
    for (int i = 0; i < g_recorder.event_count; i++) {
        cJSON *e = cJSON_CreateObject();
        cJSON_AddNumberToObject(e, "type", g_recorder.events[i].type);
        cJSON_AddNumberToObject(e, "code", g_recorder.events[i].code);
        cJSON_AddNumberToObject(e, "ch", g_recorder.events[i].ch);
        cJSON_AddItemToArray(events, e);
    }
    cJSON_AddItemToObject(root, "events", events);

    char *json = cJSON_PrintUnformatted(root);
    FILE *fp = fopen(path, "w");
    if (!fp) { cJSON_Delete(root); free(json); return false; }
    fprintf(fp, "%s\n", json);
    fclose(fp);
    free(json);
    cJSON_Delete(root);
    return true;
}

bool recorder_load(const char *path) {
    recorder_reset();
    FILE *fp = fopen(path, "r");
    if (!fp) return false;
    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    rewind(fp);
    char *json = malloc(sz + 1);
    fread(json, 1, sz, fp);
    json[sz] = '\0';
    fclose(fp);

    cJSON *root = cJSON_Parse(json);
    free(json);
    if (!root) return false;

    cJSON *frames = cJSON_GetObjectItem(root, "frames");
    cJSON *events = cJSON_GetObjectItem(root, "events");
    if (frames) {
        g_recorder.frame_count = cJSON_GetArraySize(frames);
        for (int i = 0; i < g_recorder.frame_count && i < MAX_SNAPSHOTS; i++) {
            cJSON *f = cJSON_GetArrayItem(frames, i);
            cJSON *wt = cJSON_GetObjectItem(f, "widget_type");
            cJSON *p = cJSON_GetObjectItem(f, "params");
            cJSON *r = cJSON_GetObjectItem(f, "response");
            cJSON *c = cJSON_GetObjectItem(f, "cancelled");
            g_recorder.frames[i].widget_type = wt && wt->valuestring ? strdup(wt->valuestring) : NULL;
            g_recorder.frames[i].json_params = p && p->valuestring ? strdup(p->valuestring) : NULL;
            g_recorder.frames[i].response.result = r && r->valuestring ? cJSON_Parse(r->valuestring) : NULL;
            g_recorder.frames[i].response.cancelled = c && c->valueint;
        }
    }
    if (events) {
        g_recorder.event_count = cJSON_GetArraySize(events);
        for (int i = 0; i < g_recorder.event_count && i < MAX_EVENTS; i++) {
            cJSON *e = cJSON_GetArrayItem(events, i);
            g_recorder.events[i].type = cJSON_GetObjectItem(e, "type")->valueint;
            g_recorder.events[i].code = cJSON_GetObjectItem(e, "code")->valueint;
            g_recorder.events[i].ch = cJSON_GetObjectItem(e, "ch")->valueint;
        }
    }
    cJSON_Delete(root);
    g_recorder.replaying = true;
    g_recorder.replay_pos = 0;
    return true;
}

Event recorder_next_event(void) {
    Event ev = { .type = EVENT_NONE };
    if (!g_recorder.replaying) return ev;
    if (g_recorder.replay_pos >= g_recorder.event_count) return ev;
    return g_recorder.events[g_recorder.replay_pos++];
}

bool recorder_has_events(void) {
    return g_recorder.replaying && g_recorder.replay_pos < g_recorder.event_count;
}

void recorder_reset(void) {
    for (int i = 0; i < g_recorder.frame_count; i++) {
        free(g_recorder.frames[i].widget_type);
        free(g_recorder.frames[i].json_params);
        if (g_recorder.frames[i].response.result) cJSON_Delete(g_recorder.frames[i].response.result);
        free(g_recorder.frames[i].ansi_buffer);
    }
    memset(&g_recorder, 0, sizeof(g_recorder));
}