#include "progress.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>

typedef struct {
    char *title;
    char **command;
    int cmd_count;
    char *logfile;
    char *output;
    int progress;
    char *stage;
    bool show_raw;
    bool cancelled;
    pid_t child_pid;
    int pipe_fd[2];
    bool dirty;
} ProgressData;

static void progress_start(ProgressData *d) {
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
        exit(1);
    }
    close(d->pipe_fd[1]);
    d->child_pid = pid;
}

static void progress_update(ProgressData *d) {
    if (d->child_pid == 0) return;
    char buf[4096];
    while (1) {
        ssize_t n = read(d->pipe_fd[0], buf, sizeof(buf) - 1);
        if (n <= 0) break;
        buf[n] = '\0';
        char *old = d->output;
        d->output = malloc(strlen(old) + n + 1);
        strcpy(d->output, old); strcat(d->output, buf);
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
    if (d->child_pid == 0 && d->output == NULL) progress_start(d);
    if (d->child_pid > 0) progress_update(d);
    memset(out, 0, sizeof(*out));
    int box_w = (int)(area.w * 0.8f);
    if (box_w > area.w - 2) box_w = area.w - 2;
    int box_h = (int)(area.h * 0.8f);
    if (box_h > area.h - 2) box_h = area.h - 2;
    int box_x = (area.w - box_w) / 2, box_y = (area.h - box_h) / 2;

    RenderTree *children = calloc(4 + 1, sizeof(RenderTree));
    int idx = 0;
    children[idx].type = RNODE_TEXT;
    children[idx].rect = rect_new(1, 0, box_w - 2, 1);
    children[idx].text.content = strdup(d->title);
    children[idx].text.align = ALIGN_CENTER;
    children[idx].text.style = textstyle_selected();
    idx++;

    if (d->show_raw) {
        children[idx].type = RNODE_TEXT;
        children[idx].rect = rect_new(1, 1, box_w - 2, box_h - 3);
        children[idx].text.content = d->output ? strdup(d->output) : strdup("");
        children[idx].text.align = ALIGN_LEFT;
        children[idx].text.style = textstyle_normal();
        idx++;
    } else {
        children[idx].type = RNODE_GAUGE;
        children[idx].rect = rect_new(1, 1, box_w - 2, 3);
        children[idx].gauge.percent = d->progress;
        children[idx].gauge.label = d->stage ? strdup(d->stage) : strdup("");
        children[idx].gauge.style = textstyle_accent();
        idx++;

        children[idx].type = RNODE_TEXT;
        children[idx].rect = rect_new(1, 4, box_w - 2, box_h - 6);
        char *recent = d->output ? strdup(d->output) : strdup("");
        children[idx].text.content = recent;
        children[idx].text.align = ALIGN_LEFT;
        children[idx].text.style = textstyle_muted();
        idx++;
    }

    children[idx].type = RNODE_TEXT;
    children[idx].rect = rect_new(1, box_h - 2, box_w - 2, 1);
    children[idx].text.content = strdup("[Tab] toggle raw  [Esc] cancel");
    children[idx].text.align = ALIGN_CENTER;
    children[idx].text.style = textstyle_muted();
    idx++;

    out->type = RNODE_CONTAINER;
    out->rect = rect_new(box_x, box_y, box_w, box_h);
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
            return event_result_response((WidgetResponse){ .result = NULL, .cancelled = true, .error = NULL });
        case KEY_TAB:
            d->show_raw = !d->show_raw;
            d->dirty = true;
            return event_result_handled();
        default:
            d->dirty = true;
            if (d->child_pid == 0 && d->output)
                return event_result_response((WidgetResponse){ .result = cJSON_CreateString(d->output), .cancelled = false, .error = NULL });
            return event_result_handled();
    }
}

static bool progress_is_dirty(Widget *self) { return ((ProgressData *)(self + 1))->dirty; }
static void progress_clear_dirty(Widget *self) { ((ProgressData *)(self + 1))->dirty = false; }
static void progress_destroy(Widget *self) {
    ProgressData *d = (ProgressData *)(self + 1);
    free(d->title); free(d->output);
    for (int i = 0; i < d->cmd_count; i++) free(d->command[i]);
    free(d->command); free(d->logfile);
    if (d->child_pid > 0) { kill(d->child_pid, SIGTERM); close(d->pipe_fd[0]); }
}

Widget *progress_widget_new(const char *title, char **command, int cmd_count, const char *logfile) {
    Widget *w = calloc(1, sizeof(Widget) + sizeof(ProgressData));
    w->vtable.render = progress_render;
    w->vtable.handle_event = progress_handle_event;
    w->vtable.is_dirty = progress_is_dirty;
    w->vtable.clear_dirty = progress_clear_dirty;
    w->vtable.destroy = progress_destroy;
    ProgressData *d = (ProgressData *)(w + 1);
    d->title = strdup(title);
    d->command = malloc(cmd_count * sizeof(char *));
    for (int i = 0; i < cmd_count; i++) d->command[i] = strdup(command[i]);
    d->cmd_count = cmd_count;
    d->logfile = logfile ? strdup(logfile) : NULL;
    d->output = NULL;
    d->progress = 0;
    d->stage = "Starting...";
    d->show_raw = false;
    d->cancelled = false;
    d->child_pid = 0;
    d->dirty = true;
    return w;
}