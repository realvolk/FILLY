#pragma once
#include <stdbool.h>
#include "core/store.h"

typedef struct {
    char id[64];
    Store *store;
    bool active;
    int widget_position_x;
    int widget_position_y;
    char widget_anchor[32];
    char widget_relative_to[64];
    int widget_z_index;
} Session;

bool checkpoint_save(Session *sessions, int count);
int checkpoint_restore(Session **sessions_out);