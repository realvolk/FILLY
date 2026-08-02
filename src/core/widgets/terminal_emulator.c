#include "terminal_emulator.h"
#include "core/widget_base.h"
#include "core/session.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <pty.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <fcntl.h>

#define SCROLLBACK 65536

typedef struct {
    WidgetBase base;
    char *title;
    char **command;
    int cmd_count;
    int pty_fd;
    pid_t child_pid;
    char *scrollback;
    int sb_len;
    int sb_cap;
    int term_w, term_h;
    bool running;
    char search_buf[256];
    int search_len;
    bool search_active;
} TerminalEmulatorData;

extern Arena *g_session_arena;

static void te_read_output(TerminalEmulatorData *d) {
    char buf[4096];
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(d->pty_fd, &fds);
    struct timeval tv = {0, 0};
    while (select(d->pty_fd + 1, &fds, NULL, NULL, &tv) > 0) {
        ssize_t n = read(d->pty_fd, buf, sizeof(buf) - 1);
        if (n <= 0) {
            d->running = false;
            return;
        }
        if (d->sb_len + n + 1 >= d->sb_cap) {
            d->sb_cap *= 2;
            d->scrollback = realloc(d->scrollback, d->sb_cap);
        }
        memcpy(d->scrollback + d->sb_len, buf, n);
        d->sb_len += n;
        d->scrollback[d->sb_len] = '\0';
        FD_ZERO(&fds);
        FD_SET(d->pty_fd, &fds);
        tv.tv_sec = 0;
        tv.tv_usec = 0;
    }
    int status;
    if (waitpid(d->child_pid, &status, WNOHANG) > 0) d->running = false;
}

static void terminal_emulator_render(Widget *self, RenderTree *out) {
    TerminalEmulatorData *d = (TerminalEmulatorData *)(self + 1);
    WidgetBase *base = (WidgetBase *)(self + 1);
    Rect area = base->render_area;
    d->term_w = area.w - 2;
    d->term_h = area.h - 2;
    if (d->term_w < 20) d->term_w = 20;
    if (d->term_h < 5) d->term_h = 5;

    te_read_output(d);
    memset(out, 0, sizeof(*out));
    out->style_class = "container";

    RenderTree *children = arena_alloc(g_session_arena, 3 * sizeof(RenderTree));

    children[0].type = RNODE_TEXT;
    children[0].rect = rect_new(1, 0, d->term_w, 1);
    char title_buf[512];
    snprintf(title_buf, sizeof(title_buf), "%s [%dx%d]%s", d->title, d->term_w, d->term_h,
             d->search_active ? " (search: /)" : "");
    children[0].u.text.content = arena_strdup(g_session_arena, title_buf);
    children[0].style_class = "text";
    children[0].state = "title";

    children[1].type = RNODE_TEXT;
    children[1].rect = rect_new(1, 1, d->term_w, d->term_h - 2);
    const char *display = d->scrollback;
    if (d->search_active && d->search_len > 0) {
        static char filtered[SCROLLBACK];
        int flen = 0;
        char *line_start = d->scrollback;
        while (*line_start && flen < SCROLLBACK - 256) {
            char *line_end = strchr(line_start, '\n');
            if (!line_end) line_end = line_start + strlen(line_start);
            int line_len = line_end - line_start;
            int found = 0;
            for (int off = 0; off <= line_len - d->search_len; off++) {
                if (memcmp(line_start + off, d->search_buf, d->search_len) == 0) {
                    found = 1;
                    break;
                }
            }
            if (found) {
                memcpy(filtered + flen, line_start, line_len);
                flen += line_len;
                filtered[flen++] = '\n';
            }
            line_start = (*line_end == '\n') ? line_end + 1 : line_end;
        }
        filtered[flen] = '\0';
        display = flen > 0 ? filtered : "No matches";
    }
    children[1].u.text.content = arena_strdup(g_session_arena, display);
    children[1].style_class = "text";

    children[2].type = RNODE_TEXT;
    children[2].rect = rect_new(1, d->term_h - 1, d->term_w, 1);
    if (d->search_active) {
        snprintf(title_buf, sizeof(title_buf), "/%s_", d->search_buf);
        children[2].u.text.content = arena_strdup(g_session_arena, title_buf);
    } else if (d->running) {
        children[2].u.text.content = "Ctrl+D:exit  /:search  Type:input";
    } else {
        children[2].u.text.content = "[Process ended] Any key to close";
    }
    children[2].style_class = "text";
    children[2].state = "muted";

    out->type = RNODE_CONTAINER;
    out->rect = rect_new(0, 0, area.w, area.h);
    out->u.container.border = BORDER_SINGLE;
    out->u.container.padding = edgeinsets_zero();
    out->u.container.children = children;
    out->u.container.child_count = 3;
}

static EventResult terminal_emulator_handle_event(Widget *self, Event *ev, Backend *backend) {
    (void)backend;
    TerminalEmulatorData *d = (TerminalEmulatorData *)(self + 1);

    if (!d->running && !d->search_active && ev->type == EVENT_KEY)
        return event_result_response((WidgetResponse){ .result = NULL, .cancelled = false });

    if (ev->type != EVENT_KEY) return event_result_unhandled();

    if (d->search_active) {
        switch (ev->code) {
            case KEY_ESC:
                d->search_active = false;
                d->search_len = 0;
                d->search_buf[0] = '\0';
                d->base.dirty = true;
                return event_result_handled();
            case KEY_ENTER:
                d->search_active = false;
                d->base.dirty = true;
                return event_result_handled();
            case KEY_BACKSPACE:
                if (d->search_len > 0) d->search_buf[--d->search_len] = '\0';
                d->base.dirty = true;
                return event_result_handled();
            case KEY_CHAR:
                if (ev->ch == '/') {
                    d->search_active = false;
                    d->search_len = 0;
                    d->search_buf[0] = '\0';
                    d->base.dirty = true;
                    return event_result_handled();
                }
                if (d->search_len < 254 && ev->ch >= 32) {
                    d->search_buf[d->search_len++] = ev->ch;
                    d->search_buf[d->search_len] = '\0';
                }
                d->base.dirty = true;
                return event_result_handled();
            default:
                return event_result_unhandled();
        }
    }

    switch (ev->code) {
        case KEY_ESC: write(d->pty_fd, "\x1b", 1); d->base.dirty = true; return event_result_handled();
        case KEY_ENTER: write(d->pty_fd, "\r", 1); d->base.dirty = true; return event_result_handled();
        case KEY_TAB: write(d->pty_fd, "\t", 1); d->base.dirty = true; return event_result_handled();
        case KEY_BACKSPACE: write(d->pty_fd, "\x7f", 1); d->base.dirty = true; return event_result_handled();
        case KEY_UP: write(d->pty_fd, "\x1b[A", 3); d->base.dirty = true; return event_result_handled();
        case KEY_DOWN: write(d->pty_fd, "\x1b[B", 3); d->base.dirty = true; return event_result_handled();
        case KEY_LEFT: write(d->pty_fd, "\x1b[D", 3); d->base.dirty = true; return event_result_handled();
        case KEY_RIGHT: write(d->pty_fd, "\x1b[C", 3); d->base.dirty = true; return event_result_handled();
        case KEY_HOME: write(d->pty_fd, "\x1b[H", 3); d->base.dirty = true; return event_result_handled();
        case KEY_END: write(d->pty_fd, "\x1b[F", 3); d->base.dirty = true; return event_result_handled();
        case KEY_CHAR:
            if (ev->ch == 4 && d->running) { close(d->pty_fd); d->pty_fd = -1; d->running = false; }
            else if (ev->ch == '/') { d->search_active = true; d->search_len = 0; d->search_buf[0] = '\0'; }
            else if (ev->ch >= 32) write(d->pty_fd, &ev->ch, 1);
            d->base.dirty = true;
            return event_result_handled();
        default:
            return event_result_unhandled();
    }
}

static void terminal_emulator_destroy(Widget *self) {
    TerminalEmulatorData *d = (TerminalEmulatorData *)(self + 1);
    if (d->pty_fd >= 0) close(d->pty_fd);
    if (d->child_pid > 0) { kill(d->child_pid, SIGTERM); waitpid(d->child_pid, NULL, 0); }
    free(d->title);
    for (int i = 0; i < d->cmd_count; i++) free(d->command[i]);
    free(d->command);
    free(d->scrollback);
}

Widget *terminal_emulator_widget_new(const char *title, char **command, int cmd_count) {
    Widget *w = calloc(1, sizeof(Widget) + sizeof(TerminalEmulatorData));
    TerminalEmulatorData *d = (TerminalEmulatorData *)(w + 1);
    d->base.dirty = true;
    d->base.accepts_text_input = true;
    d->title = title ? strdup(title) : strdup("Terminal");
    d->cmd_count = cmd_count;
    d->command = malloc(cmd_count * sizeof(char *));
    for (int i = 0; i < cmd_count; i++) d->command[i] = strdup(command[i]);
    d->sb_cap = SCROLLBACK;
    d->scrollback = malloc(d->sb_cap);
    d->sb_len = 0;
    d->scrollback[0] = '\0';
    d->term_w = 80;
    d->term_h = 24;
    d->search_active = false;
    d->search_len = 0;
    d->search_buf[0] = '\0';

    char *argv[cmd_count + 1];
    for (int i = 0; i < cmd_count; i++) argv[i] = command[i];
    argv[cmd_count] = NULL;

    struct winsize ws = { .ws_row = d->term_h, .ws_col = d->term_w };
    d->child_pid = forkpty(&d->pty_fd, NULL, NULL, &ws);
    if (d->child_pid == 0) { execvp(argv[0], argv); _exit(1); }
    if (d->pty_fd >= 0) {
        int flags = fcntl(d->pty_fd, F_GETFL, 0);
        fcntl(d->pty_fd, F_SETFL, flags | O_NONBLOCK);
    }
    d->running = (d->pty_fd >= 0 && d->child_pid > 0);

    w->vtable.render = terminal_emulator_render;
    w->vtable.handle_event = terminal_emulator_handle_event;
    w->vtable.is_dirty = widget_base_is_dirty;
    w->vtable.clear_dirty = widget_base_clear_dirty;
    w->vtable.destroy = terminal_emulator_destroy;
    return w;
}