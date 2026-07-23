#include "progress.h"
#include "core/widget_base.h"
#include "core/session.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <limits.h>

typedef struct { WidgetBase base; char *title, **command, *output, *stage, *logfile; int cmd_count, progress, pipe_fd[2]; pid_t child_pid; bool show_raw; } ProgressData;
extern Arena *g_session_arena;

static bool progress_command_allowed(char **cmd, int count) {
    if (count < 1 || !cmd[0]) return false;
    char resolved[PATH_MAX];
    if (!realpath(cmd[0], resolved)) return false;
    const char *allowed[] = {"/usr/bin/", "/usr/sbin/", "/bin/", "/sbin/", NULL};
    for (int i = 0; allowed[i]; i++) {
        size_t len = strlen(allowed[i]);
        if (strncmp(resolved, allowed[i], len) == 0 && (resolved[len] == '/' || resolved[len] == '\0')) return true;
    }
    return false;
}

static void progress_start(ProgressData *d) {
    if (!progress_command_allowed(d->command, d->cmd_count)) {
        d->output = strdup("Command not allowed\n");
        return;
    }
    if (pipe(d->pipe_fd) < 0) return;
    pid_t pid = fork();
    if (pid == 0) {
        close(d->pipe_fd[0]);
        dup2(d->pipe_fd[1], STDOUT_FILENO);
        dup2(d->pipe_fd[1], STDERR_FILENO);
        char **argv = malloc((d->cmd_count + 1) * sizeof(char *));
        for (int i = 0; i < d->cmd_count; i++) argv[i] = d->command[i];
        argv[d->cmd_count] = NULL;
        execvp(argv[0], argv);
        _exit(1);
    }
    close(d->pipe_fd[1]);
    d->child_pid = pid;
}

static void progress_update(ProgressData *d) {
    if (d->child_pid == 0) return;
    char buf[4096];
    ssize_t n = read(d->pipe_fd[0], buf, sizeof(buf) - 1);
    if (n > 0) {
        buf[n] = '\0';
        char *old = d->output;
        d->output = malloc(strlen(old) + n + 1);
        strcpy(d->output, old);
        strcat(d->output, buf);
        free(old);
        if (strstr(buf, "Preflight")) { d->progress = 5; d->stage = "Preflight complete"; }
        else if (strstr(buf, "Mount")) { d->progress = 20; d->stage = "Storage configured"; }
        else if (strstr(buf, "Base system")) { d->progress = 50; d->stage = "Base installed"; }
        else if (strstr(buf, "Bootloader")) { d->progress = 78; d->stage = "Bootloader configured"; }
        else if (strstr(buf, "Post-install")) { d->progress = 90; d->stage = "Post-install done"; }
        else if (strstr(buf, "Final")) { d->progress = 100; d->stage = "Finalizing"; }
    }
    int status;
    if (waitpid(d->child_pid, &status, WNOHANG) > 0) {
        d->progress = 100;
        d->stage = "Complete";
        close(d->pipe_fd[0]);
        d->child_pid = 0;
    }
}

static void progress_render(Widget *self, Rect area, RenderTree *out) {
    ProgressData *d = (ProgressData *)(self + 1);
    if (d->child_pid == 0 && d->output && strlen(d->output) == 0) progress_start(d);
    if (d->child_pid > 0) progress_update(d);
    memset(out, 0, sizeof(*out));
    out->style_class = "container";
    int box_w = (int)(area.w * 0.8f);
    if (box_w > area.w - 2) box_w = area.w - 2;
    int box_h = (int)(area.h * 0.8f);
    if (box_h > area.h - 2) box_h = area.h - 2;
    RenderTree *children = arena_alloc(g_session_arena, 4 * sizeof(RenderTree));
    int idx = 0;
    children[idx].type = RNODE_TEXT;
    children[idx].rect = rect_new(1, 0, box_w - 2, 1);
    children[idx].text.content = arena_strdup(g_session_arena, d->title);
    children[idx].style_class = "text";
    children[idx].state = "title";
    idx++;
    if (d->show_raw) {
        children[idx].type = RNODE_TEXT;
        children[idx].rect = rect_new(1, 1, box_w - 2, box_h - 3);
        children[idx].text.content = arena_strdup(g_session_arena, d->output ? d->output : "");
        children[idx].style_class = "text";
        idx++;
    } else {
        children[idx].type = RNODE_GAUGE;
        children[idx].rect = rect_new(1, 1, box_w - 2, 3);
        children[idx].gauge.percent = d->progress;
        children[idx].gauge.label = arena_strdup(g_session_arena, d->stage ? d->stage : "");
        children[idx].style_class = "gauge";
        idx++;
        children[idx].type = RNODE_TEXT;
        children[idx].rect = rect_new(1, 4, box_w - 2, box_h - 6);
        children[idx].text.content = arena_strdup(g_session_arena, d->output ? d->output : "");
        children[idx].style_class = "text";
        idx++;
    }
    children[idx].type = RNODE_TEXT;
    children[idx].rect = rect_new(1, box_h - 2, box_w - 2, 1);
    children[idx].text.content = "[Tab] toggle raw  [Esc] cancel";
    children[idx].style_class = "text";
    children[idx].state = "muted";
    idx++;
    out->type = RNODE_CONTAINER;
    out->rect = rect_new((area.w - box_w) / 2, (area.h - box_h) / 2, box_w, box_h);
    out->container.border = BORDER_SINGLE;
    out->container.padding = edgeinsets_zero();
    out->container.children = children;
    out->container.child_count = idx;
}

static EventResult progress_handle_event(Widget *self, Event *ev, Backend *backend) {
    (void)backend;
    ProgressData *d = (ProgressData *)(self + 1);
    if (ev->type != EVENT_KEY) return event_result_handled();
    switch (ev->code) {
        case KEY_ESC:
            if (d->child_pid > 0) { kill(d->child_pid, SIGTERM); d->child_pid = 0; }
            return event_result_response((WidgetResponse){ .result = NULL, .cancelled = true });
        case KEY_TAB:
            d->show_raw = !d->show_raw;
            d->base.dirty = true;
            return event_result_handled();
        default:
            d->base.dirty = true;
            if (d->child_pid == 0 && d->output && strlen(d->output) > 0)
                return event_result_response((WidgetResponse){ .result = cJSON_CreateString(d->output), .cancelled = false });
            return event_result_handled();
    }
}

static void progress_destroy(Widget *self) {
    ProgressData *d = (ProgressData *)(self + 1);
    free(d->title);
    free(d->output);
    for (int i = 0; i < d->cmd_count; i++) free(d->command[i]);
    free(d->command);
    free(d->logfile);
    if (d->child_pid > 0) { kill(d->child_pid, SIGTERM); close(d->pipe_fd[0]); }
}

Widget *progress_widget_new(const char *title, char **command, int cmd_count, const char *logfile) {
    Widget *w = calloc(1, sizeof(Widget) + sizeof(ProgressData));
    ProgressData data = {
        .title = strdup(title),
        .cmd_count = cmd_count,
        .output = strdup(""),
        .progress = 0,
        .stage = "Starting...",
        .show_raw = false,
        .child_pid = 0,
        .logfile = logfile ? strdup(logfile) : NULL
    };
    data.command = malloc(cmd_count * sizeof(char *));
    for (int i = 0; i < cmd_count; i++) data.command[i] = strdup(command[i]);
    widget_base_init(w, &data, sizeof(ProgressData), progress_render, progress_handle_event, progress_destroy);
    return w;
}