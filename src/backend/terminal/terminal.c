#include "terminal.h"
#include "renderer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/time.h>
#include <signal.h>

static volatile sig_atomic_t term_resized = 0;
static void handle_sigwinch(int sig) { (void)sig; term_resized = 1; }

static int write_all(int fd, const char *buf, int len) {
    int written = 0;
    while (written < len) {
        int n = write(fd, buf + written, len - written);
        if (n < 0) return -1;
        written += n;
    }
    return written;
}

static bool term_setup(void *self) {
    TerminalBackend *t = (TerminalBackend *)self;
    if (t->session_active) return true;

    struct termios raw;
    tcgetattr(t->tty_fd, &raw);
    t->orig_flags = raw.c_lflag;
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_iflag &= ~(IXON | ICRNL | BRKINT | INPCK | ISTRIP);
    raw.c_cflag |= CS8;
    raw.c_oflag &= ~OPOST;
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;
    tcsetattr(t->tty_fd, TCSAFLUSH, &raw);
    t->raw_mode = true;

    write_all(t->tty_fd, "\033[?25l", 6);
    const char *term = getenv("TERM");
    bool no_mouse = (term && (strncmp(term, "linux", 5) == 0 || strcmp(term, "vt100") == 0 || strcmp(term, "vt102") == 0));
    if (!no_mouse) {
        write_all(t->tty_fd, "\033[?1000h\033[?1006h", 16);
    }
    t->session_active = true;
    signal(SIGWINCH, handle_sigwinch);
    return true;
}

static bool term_draw(void *self, RenderTree *tree) {
    TerminalBackend *t = (TerminalBackend *)self;
    int w, h;
    struct winsize ws;
    if (ioctl(t->tty_fd, TIOCGWINSZ, &ws) == 0) { w = ws.ws_col; h = ws.ws_row; }
    else { w = 80; h = 24; }
    char buf[524288];
    render_tree_to_buf(tree, 0, 0, w, h, buf, sizeof(buf));
    write_all(t->tty_fd, buf, strlen(buf));
    return true;
}

static void term_get_size(void *self, int *w, int *h) {
    TerminalBackend *t = (TerminalBackend *)self;
    struct winsize ws;
    if (ioctl(t->tty_fd, TIOCGWINSZ, &ws) == 0) { *w = ws.ws_col; *h = ws.ws_row; }
    else { *w = 80; *h = 24; }
    if (term_resized) {
        if (ioctl(t->tty_fd, TIOCGWINSZ, &ws) == 0) { *w = ws.ws_col; *h = ws.ws_row; }
        term_resized = 0;
    }
}

static int read_byte_timeout(int fd, int timeout_ms) {
    fd_set set;
    FD_ZERO(&set);
    FD_SET(fd, &set);
    struct timeval tv = { .tv_sec = 0, .tv_usec = timeout_ms * 1000 };
    if (select(fd + 1, &set, NULL, NULL, &tv) <= 0) return -1;
    unsigned char c;
    if (read(fd, &c, 1) != 1) return -1;
    return c;
}

static Event term_next_event(void *self) {
    TerminalBackend *t = (TerminalBackend *)self;
    Event ev = { .type = EVENT_NONE };

    int c = read_byte_timeout(t->tty_fd, 100);
    if (c < 0) return ev;

    if (c != '\033') {
        if (c == '\t') { ev.type = EVENT_KEY; ev.code = KEY_TAB; return ev; }
        if (c == '\r' || c == '\n') { ev.type = EVENT_KEY; ev.code = KEY_ENTER; return ev; }
        if (c == 127) { ev.type = EVENT_KEY; ev.code = KEY_BACKSPACE; return ev; }
        ev.type = EVENT_KEY;
        ev.code = KEY_CHAR;
        ev.ch = c;
        return ev;
    }

    int c2 = read_byte_timeout(t->tty_fd, 10);
    if (c2 < 0) {
        ev.type = EVENT_KEY;
        ev.code = KEY_ESC;
        return ev;
    }

    if (c2 == '[') {
        int c3 = read_byte_timeout(t->tty_fd, 20);
        if (c3 < 0) { ev.type = EVENT_KEY; ev.code = KEY_ESC; return ev; }

        if (c3 == '<') {
            char mouse_buf[32]; int mi = 0;
            int c4;
            while ((c4 = read_byte_timeout(t->tty_fd, 10)) > 0 && mi < 30) {
                mouse_buf[mi++] = c4;
                if (c4 == 'M' || c4 == 'm') break;
            }
            mouse_buf[mi] = '\0';
            int btn = 0, mx = 0, my = 0;
            sscanf(mouse_buf, "%d;%d;%d", &btn, &mx, &my);
            ev.type = EVENT_MOUSE_BUTTON;
            ev.x = mx - 1;
            ev.y = my - 1;
            ev.button = btn & 0x03;
            ev.mouse_state = (btn & 0x20) ? MOUSE_MOTION : ((c4 == 'M') ? MOUSE_PRESS : MOUSE_RELEASE);
            return ev;
        }

        char params[16] = {0};
        int pi = 0;
        while (pi < 15) {
            if (c3 >= '0' && c3 <= '9') {
                if (pi < 15) params[pi++] = c3;
            } else if (c3 == ';') {
                if (pi < 15) params[pi++] = c3;
            } else if ((c3 >= 'A' && c3 <= 'Z') || (c3 >= 'a' && c3 <= 'z') || c3 == '~') {
                params[pi] = '\0';
                if (pi == 0) {
                    switch (c3) {
                        case 'A': ev.type = EVENT_KEY; ev.code = KEY_UP; break;
                        case 'B': ev.type = EVENT_KEY; ev.code = KEY_DOWN; break;
                        case 'C': ev.type = EVENT_KEY; ev.code = KEY_RIGHT; break;
                        case 'D': ev.type = EVENT_KEY; ev.code = KEY_LEFT; break;
                        case 'H': ev.type = EVENT_KEY; ev.code = KEY_HOME; break;
                        case 'F': ev.type = EVENT_KEY; ev.code = KEY_END; break;
                        case 'Z': ev.type = EVENT_KEY; ev.code = KEY_BACKTAB; break;
                    }
                } else {
                    int p1 = 0;
                    char *semi = strchr(params, ';');
                    if (semi) *semi = '\0';
                    p1 = atoi(params);
                    if (c3 == '~') {
                        switch (p1) {
                            case 1: case 7: ev.type = EVENT_KEY; ev.code = KEY_HOME; break;
                            case 2: ev.type = EVENT_KEY; ev.code = KEY_INSERT; break;
                            case 3: ev.type = EVENT_KEY; ev.code = KEY_DELETE; break;
                            case 4: case 8: ev.type = EVENT_KEY; ev.code = KEY_END; break;
                            case 5: ev.type = EVENT_KEY; ev.code = KEY_PAGEUP; break;
                            case 6: ev.type = EVENT_KEY; ev.code = KEY_PAGEDOWN; break;
                            case 11: ev.type = EVENT_KEY; ev.code = KEY_F1; break;
                            case 12: ev.type = EVENT_KEY; ev.code = KEY_F2; break;
                            case 13: ev.type = EVENT_KEY; ev.code = KEY_F3; break;
                            case 14: ev.type = EVENT_KEY; ev.code = KEY_F4; break;
                            case 15: ev.type = EVENT_KEY; ev.code = KEY_F5; break;
                            case 17: ev.type = EVENT_KEY; ev.code = KEY_F6; break;
                            case 18: ev.type = EVENT_KEY; ev.code = KEY_F7; break;
                            case 19: ev.type = EVENT_KEY; ev.code = KEY_F8; break;
                            case 20: ev.type = EVENT_KEY; ev.code = KEY_F9; break;
                            case 21: ev.type = EVENT_KEY; ev.code = KEY_F10; break;
                            case 23: ev.type = EVENT_KEY; ev.code = KEY_F11; break;
                            case 24: ev.type = EVENT_KEY; ev.code = KEY_F12; break;
                        }
                    }
                }
                return ev;
            }
            c3 = read_byte_timeout(t->tty_fd, 20);
            if (c3 < 0) break;
        }
        return ev;
    }

    if (c2 == 'O') {
        int c3 = read_byte_timeout(t->tty_fd, 10);
        if (c3 < 0) { ev.type = EVENT_KEY; ev.code = KEY_ESC; return ev; }
        switch (c3) {
            case 'P': ev.type = EVENT_KEY; ev.code = KEY_F1; break;
            case 'Q': ev.type = EVENT_KEY; ev.code = KEY_F2; break;
            case 'R': ev.type = EVENT_KEY; ev.code = KEY_F3; break;
            case 'S': ev.type = EVENT_KEY; ev.code = KEY_F4; break;
        }
        return ev;
    }

    ev.type = EVENT_KEY;
    ev.code = KEY_ESC;
    return ev;
}

static bool term_teardown(void *self) {
    TerminalBackend *t = (TerminalBackend *)self;
    if (!t->session_active) return true;

    const char *term = getenv("TERM");
    bool no_mouse = (term && (strncmp(term, "linux", 5) == 0 ||
                              strcmp(term, "vt100") == 0 ||
                              strcmp(term, "vt102") == 0));

    if (t->raw_mode) {
        struct termios normal;
        tcgetattr(t->tty_fd, &normal);
        normal.c_lflag = t->orig_flags;
        tcsetattr(t->tty_fd, TCSADRAIN, &normal);
        t->raw_mode = false;
    }

    if (no_mouse) {
        write_all(t->tty_fd, "\033[0m\033[2J\033[H\033[?25h", 15);
    } else {
        write_all(t->tty_fd, "\033[0m\033[2J\033[H\033[?25h\033[?1000l\033[?1006l", 29);
    }
    t->session_active = false;
    return true;
}

BackendVTable terminal_vtable = {
    .setup = term_setup,
    .draw = term_draw,
    .next_event = term_next_event,
    .teardown = term_teardown,
    .get_size = term_get_size,
    .wait_frame = NULL,
    .is_interactive = true,
};

bool terminal_backend_init(TerminalBackend *t, const char *tty_path) {
    if (!isatty(STDIN_FILENO)) {
        return false;
    }
    if (tty_path && strlen(tty_path) > 0) {
        t->tty_fd = open(tty_path, O_RDWR);
    } else {
        t->tty_fd = open("/dev/tty", O_RDWR);
    }
    if (t->tty_fd < 0) return false;
    t->raw_mode = false;
    t->alt_screen = false;
    t->session_active = false;
    t->tty_path = tty_path ? strdup(tty_path) : strdup("/dev/tty");
    return true;
}

void terminal_backend_destroy(TerminalBackend *t) {
    if (t->tty_fd >= 0) close(t->tty_fd);
    free(t->tty_path);
}

Backend *terminal_backend_new(void) {
    TerminalBackend *t = malloc(sizeof(TerminalBackend));
    if (!t) return NULL;
    if (!terminal_backend_init(t, NULL)) { free(t); return NULL; }
    Backend *b = malloc(sizeof(Backend));
    b->vtable = &terminal_vtable;
    b->data = t;
    return b;
}