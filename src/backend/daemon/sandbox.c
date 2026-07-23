#include "sandbox.h"
#include "core/log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <dlfcn.h>
#ifdef __linux__
#include <seccomp.h>
#endif

bool sandbox_spawn(const char *so_path, const char *widget_name, const char *json_params, SandboxHandle *handle) {
    (void)widget_name;
    (void)json_params;
    if (pipe(handle->pipe_fd) < 0) {
        LOG_ERROR("sandbox: pipe failed");
        return false;
    }

    pid_t pid = fork();
    if (pid < 0) {
        LOG_ERROR("sandbox: fork failed");
        close(handle->pipe_fd[0]);
        close(handle->pipe_fd[1]);
        return false;
    }

    if (pid == 0) {
        close(handle->pipe_fd[0]);

        struct rlimit limit;
        limit.rlim_cur = 128 * 1024 * 1024;
        limit.rlim_max = 128 * 1024 * 1024;
        setrlimit(RLIMIT_AS, &limit);
        limit.rlim_cur = 2;
        limit.rlim_max = 2;
        setrlimit(RLIMIT_CPU, &limit);

#ifdef __linux__
        scmp_filter_ctx ctx = seccomp_init(SCMP_ACT_KILL);
        if (ctx) {
            seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(write), 0);
            seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(close), 0);
            seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(exit), 0);
            seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(exit_group), 0);
            seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(brk), 0);
            seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(mmap), 0);
            seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(munmap), 0);
            seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(mprotect), 0);
            seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(fstat), 0);
            seccomp_load(ctx);
            seccomp_release(ctx);
        }
#endif

        void *lib = dlopen(so_path, RTLD_NOW);
        if (!lib) {
            const char *err = dlerror();
            write(handle->pipe_fd[1], err, strlen(err));
            write(handle->pipe_fd[1], "\n", 1);
            _exit(1);
        }

        void (*reg)(void (*)(const char *, WidgetFactory));
        *(void **)(&reg) = dlsym(lib, "register_plugins");
        if (!reg) {
            write(handle->pipe_fd[1], "No register_plugins symbol\n", 27);
            _exit(1);
        }

        _exit(0);
    }

    handle->child_pid = pid;
    close(handle->pipe_fd[1]);
    LOG_INFO("sandbox: spawned child %d for %s", pid, so_path);
    return true;
}

RenderTree *sandbox_get_result(SandboxHandle *handle) {
    if (handle->child_pid == 0) return NULL;
    char buf[65536];
    ssize_t n = read(handle->pipe_fd[0], buf, sizeof(buf) - 1);
    if (n <= 0) {
        int status;
        waitpid(handle->child_pid, &status, WNOHANG);
        return NULL;
    }
    buf[n] = '\0';
    int status;
    waitpid(handle->child_pid, &status, 0);
    handle->child_pid = 0;
    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        LOG_INFO("sandbox: plugin completed successfully");
        return NULL;
    }
    LOG_WARN("sandbox: plugin failed: %s", buf);
    return NULL;
}

void sandbox_cleanup(SandboxHandle *handle) {
    if (handle->child_pid > 0) {
        kill(handle->child_pid, SIGKILL);
        waitpid(handle->child_pid, NULL, 0);
    }
    close(handle->pipe_fd[0]);
    close(handle->pipe_fd[1]);
    memset(handle, 0, sizeof(*handle));
}