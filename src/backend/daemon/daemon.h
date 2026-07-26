#pragma once
#include "core/store.h"
#include "protocol/protocol.h"
#include "checkpoint.h"

void load_plugins(void);
bool daemon_run(const char *socket_path, const char *theme_path);
void set_insecure_plugins(bool val);
void set_use_sandbox(bool val);
void register_builtin_widgets(void);