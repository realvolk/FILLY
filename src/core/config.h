#pragma once
#include <stdbool.h>

typedef struct {
    char socket_path[256];
    char theme[256];
    char plugin_dir[256];
    char log_path[256];
    int log_level;
    int inactivity_timeout;
    bool sandbox;
    int max_connections_per_sec;
} FillyConfig;

void config_load(FillyConfig *cfg, const char *path);
void config_defaults(FillyConfig *cfg);