#include "config.h"
#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void config_defaults(FillyConfig *cfg) {
    memset(cfg, 0, sizeof(*cfg));
    const char *xdg = getenv("XDG_RUNTIME_DIR");
    if (xdg) snprintf(cfg->socket_path, sizeof(cfg->socket_path), "%s/filly.sock", xdg);
    else strcpy(cfg->socket_path, "/tmp/filly.sock");
    strcpy(cfg->theme, "themes/forge.json");
    const char *home = getenv("HOME");
    if (home) snprintf(cfg->plugin_dir, sizeof(cfg->plugin_dir), "%s/.config/filly/plugins", home);
    else strcpy(cfg->plugin_dir, "/usr/local/share/filly/plugins");
    cfg->log_path[0] = '\0';
    cfg->log_level = 1; /* INFO */
    cfg->inactivity_timeout = 30;
    cfg->sandbox = false;
    cfg->max_connections_per_sec = 10;
}

void config_load(FillyConfig *cfg, const char *path) {
    config_defaults(cfg);
    if (!path) {
        const char *home = getenv("HOME");
        static char default_path[1024];
        if (home) snprintf(default_path, sizeof(default_path), "%s/.config/filly/filly.conf", home);
        else return;
        path = default_path;
    }
    FILE *f = fopen(path, "r");
    if (!f) return;

    char line[512];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\n")] = 0;
        if (line[0] == '#' || line[0] == '\0') continue;
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        char *key = line;
        char *val = eq + 1;
        while (*key == ' ') key++;
        while (*val == ' ') val++;

        if (strcmp(key, "socket_path") == 0) strncpy(cfg->socket_path, val, sizeof(cfg->socket_path)-1);
        else if (strcmp(key, "theme") == 0) strncpy(cfg->theme, val, sizeof(cfg->theme)-1);
        else if (strcmp(key, "plugin_dir") == 0) strncpy(cfg->plugin_dir, val, sizeof(cfg->plugin_dir)-1);
        else if (strcmp(key, "log_path") == 0) strncpy(cfg->log_path, val, sizeof(cfg->log_path)-1);
        else if (strcmp(key, "log_level") == 0) cfg->log_level = atoi(val);
        else if (strcmp(key, "inactivity_timeout") == 0) cfg->inactivity_timeout = atoi(val);
        else if (strcmp(key, "sandbox") == 0) cfg->sandbox = strcmp(val, "true") == 0 || strcmp(val, "1") == 0;
        else if (strcmp(key, "max_connections_per_sec") == 0) cfg->max_connections_per_sec = atoi(val);
    }
    fclose(f);
}