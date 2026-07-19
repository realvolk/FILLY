#pragma once
#include "../filly-protocol/protocol.h"
#include "checkpoint.h"

void load_plugins(void);
bool daemon_run(const char *socket_path);
void set_insecure_plugins(bool val);