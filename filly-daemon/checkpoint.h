#pragma once
#include <stdbool.h>
#include "../filly-core/store.h"

typedef struct {
    char id[64];
    Store *store;
    bool active;
} Session;

bool checkpoint_save(Session *sessions, int count);
int checkpoint_restore(Session **sessions_out);