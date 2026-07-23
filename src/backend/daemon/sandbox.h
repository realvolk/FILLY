#pragma once
#include <stdbool.h>
#include <sys/types.h>
#include "core/render.h"
#include "core/widget.h"

typedef struct {
    int pipe_fd[2];
    pid_t child_pid;
} SandboxHandle;

bool sandbox_spawn(const char *so_path, const char *widget_name, const char *json_params, SandboxHandle *handle);
RenderTree *sandbox_get_result(SandboxHandle *handle);
void sandbox_cleanup(SandboxHandle *handle);