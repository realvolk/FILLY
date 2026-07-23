#pragma once
#include "cJSON.h"
#include <stdbool.h>

typedef struct FillyClient FillyClient;

typedef void (*FillyDrawCallback)(const char *data, int len, void *user);

FillyClient *filly_client_connect(const char *socket_path);
void         filly_client_disconnect(FillyClient *c);
int          filly_client_send_request(FillyClient *c, const char *json);
int          filly_client_send_key(FillyClient *c, int keycode, char ch);
int          filly_client_send_quit(FillyClient *c);
int          filly_client_poll(FillyClient *c, int timeout_ms);
int          filly_client_get_response(FillyClient *c, cJSON **result, bool *cancelled);
void         filly_client_set_draw_callback(FillyClient *c, FillyDrawCallback cb, void *user);
int          filly_client_get_fd(FillyClient *c);
int          filly_client_process(FillyClient *c);
bool         filly_client_has_response(FillyClient *c);