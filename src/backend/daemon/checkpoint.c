#include "checkpoint.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <ctype.h>

static bool is_sensitive_key(const char *key) {
    char *upper = strdup(key);
    for (int i = 0; upper[i]; i++) upper[i] = toupper(upper[i]);
    bool sensitive = strstr(upper, "PASS") || strstr(upper, "SECRET") ||
                     strstr(upper, "LUKS") || strstr(upper, "TOKEN") ||
                     strstr(upper, "KEY");
    free(upper);
    return sensitive;
}

bool checkpoint_save(Session *sessions, int count) {
    const char *home = getenv("HOME");
    if (!home) return false;
    char dir[1024];
    snprintf(dir, sizeof(dir), "%s/.cache/filly", home);
    mkdir(dir, 0700);
    char filepath[2048];
    snprintf(filepath, sizeof(filepath), "%s/checkpoint.json", dir);
    char tmpfile[2048];
    snprintf(tmpfile, sizeof(tmpfile), "%s.tmp", filepath);

    FILE *fp = fopen(tmpfile, "w");
    if (!fp) return false;
    fchmod(fileno(fp), 0600);

    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "version", 1);
    cJSON *arr = cJSON_CreateArray();
    for (int i = 0; i < count; i++) {
        if (!sessions[i].active) continue;
        cJSON *obj = cJSON_CreateObject();
        cJSON_AddStringToObject(obj, "id", sessions[i].id);
        cJSON *store_json = cJSON_CreateObject();
        const char *key = NULL;
        const char *val = NULL;
        int idx = 0;
        while (store_enum(sessions[i].store, &idx, &key, &val)) {
            if (!is_sensitive_key(key))
                cJSON_AddStringToObject(store_json, key, val);
        }
        cJSON_AddItemToObject(obj, "store", store_json);
        cJSON_AddItemToArray(arr, obj);
    }
    cJSON_AddItemToObject(root, "sessions", arr);
    char *json = cJSON_PrintUnformatted(root);
    fprintf(fp, "%s\n", json);
    fclose(fp);
    free(json);
    cJSON_Delete(root);
    rename(tmpfile, filepath);
    return true;
}

int checkpoint_restore(Session **sessions_out) {
    const char *home = getenv("HOME");
    if (!home) return 0;
    char filepath[2048];
    snprintf(filepath, sizeof(filepath), "%s/.cache/filly/checkpoint.json", home);
    FILE *fp = fopen(filepath, "r");
    if (!fp) return 0;
    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    if (sz <= 0) { fclose(fp); unlink(filepath); return 0; }
    rewind(fp);
    char *buf = malloc(sz + 1);
    fread(buf, 1, sz, fp);
    buf[sz] = '\0';
    fclose(fp);
    unlink(filepath);
    cJSON *root = cJSON_Parse(buf);
    free(buf);
    if (!root) return 0;
    cJSON *arr = cJSON_GetObjectItem(root, "sessions");
    if (!arr || !cJSON_IsArray(arr)) { cJSON_Delete(root); return 0; }
    int count = cJSON_GetArraySize(arr);
    Session *sessions = calloc(count, sizeof(Session));
    int restored = 0;
    for (int i = 0; i < count; i++) {
        cJSON *obj = cJSON_GetArrayItem(arr, i);
        cJSON *id = cJSON_GetObjectItem(obj, "id");
        cJSON *store_json = cJSON_GetObjectItem(obj, "store");
        if (!id || !id->valuestring) continue;
        strncpy(sessions[restored].id, id->valuestring, sizeof(sessions[restored].id)-1);
        sessions[restored].store = store_new();
        if (store_json && cJSON_IsObject(store_json)) {
            cJSON *child = store_json->child;
            while (child) {
                if (child->valuestring)
                    store_set(sessions[restored].store, child->string, child->valuestring);
                child = child->next;
            }
        }
        sessions[restored].active = true;
        restored++;
    }
    cJSON_Delete(root);
    *sessions_out = sessions;
    return restored;
}