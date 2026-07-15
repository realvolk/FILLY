#pragma once
#include "../filly-protocol/protocol.h"

void load_plugins(void);
bool daemon_run(const char *socket_path);