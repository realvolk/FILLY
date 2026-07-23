#include "relay.h"
#include "core/client.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <stdbool.h>
#include "core/event.h"

static struct termios orig_termios;
static int tty_fd = -1;

static void relay_teardown(void) {
    if (tty_fd >= 0) {
        write(tty_fd, "\033[0m\033[?1049l\033[?25h", 17);
        tcsetattr(tty_fd, TCSAFLUSH, &orig_termios);
        close(tty_fd);
        tty_fd = -1;
    }
}

static bool relay_setup(void) {
    if (access("/dev/tty", R_OK | W_OK) != 0) return false;
    tty_fd = open("/dev/tty", O_RDWR);
    if (tty_fd < 0) return false;
    tcgetattr(tty_fd, &orig_termios);
    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_iflag &= ~(IXON | ICRNL | BRKINT | INPCK | ISTRIP);
    raw.c_cflag |= CS8;
    raw.c_oflag &= ~OPOST;
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;
    tcsetattr(tty_fd, TCSAFLUSH, &raw);

    const char *term = getenv("TERM");
    bool no_mouse = (term && (strncmp(term, "linux", 5) == 0 ||
                              strcmp(term, "vt100") == 0 ||
                              strcmp(term, "vt102") == 0));
    if (no_mouse) {
        write(tty_fd, "\033[?1049h\033[?25l", 14);
    } else {
        write(tty_fd, "\033[?1049h\033[?25l\033[?1000h\033[?1006h", 28);
    }

    return true;
}

static int parse_csi(int fd, int *code) {
    unsigned char c;
    if (read(fd, &c, 1) != 1) return -1;
    if (c == '[') {
        char params[16] = {0}; int pi = 0; unsigned char c3;
        fd_set s; FD_ZERO(&s); FD_SET(fd, &s);
        struct timeval tv = {0, 20000};
        if (select(fd+1, &s, NULL, NULL, &tv) <= 0) return -1;
        if (read(fd, &c3, 1) != 1) return -1;
        while (pi < 15) {
            if (c3 >= '0' && c3 <= '9') { if (pi < 15) params[pi++] = c3; }
            else if (c3 == ';') { if (pi < 15) params[pi++] = c3; }
            else if ((c3 >= 'A' && c3 <= 'Z') || (c3 >= 'a' && c3 <= 'z') || c3 == '~') {
                params[pi] = 0;
                if (pi == 0) {
                    switch (c3) {
                        case 'A': *code = KEY_UP; return 0;
                        case 'B': *code = KEY_DOWN; return 0;
                        case 'C': *code = KEY_RIGHT; return 0;
                        case 'D': *code = KEY_LEFT; return 0;
                        case 'H': *code = KEY_HOME; return 0;
                        case 'F': *code = KEY_END; return 0;
                        case 'Z': *code = KEY_BACKTAB; return 0;
                    }
                } else if (c3 == '~') {
                    int p1 = atoi(params);
                    switch (p1) {
                        case 1: case 7: *code = KEY_HOME; return 0;
                        case 2: *code = KEY_INSERT; return 0;
                        case 3: *code = KEY_DELETE; return 0;
                        case 4: case 8: *code = KEY_END; return 0;
                        case 5: *code = KEY_PAGEUP; return 0;
                        case 6: *code = KEY_PAGEDOWN; return 0;
                        case 11: *code = KEY_F1; return 0;
                        case 12: *code = KEY_F2; return 0;
                        case 13: *code = KEY_F3; return 0;
                        case 14: *code = KEY_F4; return 0;
                        case 15: *code = KEY_F5; return 0;
                        case 17: *code = KEY_F6; return 0;
                        case 18: *code = KEY_F7; return 0;
                        case 19: *code = KEY_F8; return 0;
                        case 20: *code = KEY_F9; return 0;
                        case 21: *code = KEY_F10; return 0;
                        case 23: *code = KEY_F11; return 0;
                        case 24: *code = KEY_F12; return 0;
                    }
                }
                return -1;
            }
            FD_ZERO(&s); FD_SET(fd, &s);
            tv.tv_usec = 20000;
            if (select(fd+1, &s, NULL, NULL, &tv) <= 0) return -1;
            if (read(fd, &c3, 1) != 1) return -1;
        }
        return -1;
    }
    if (c == 'O') {
        unsigned char c3;
        fd_set s; FD_ZERO(&s); FD_SET(fd, &s);
        struct timeval tv = {0, 10000};
        if (select(fd+1, &s, NULL, NULL, &tv) <= 0) return -1;
        if (read(fd, &c3, 1) != 1) return -1;
        switch (c3) {
            case 'P': *code = KEY_F1; return 0;
            case 'Q': *code = KEY_F2; return 0;
            case 'R': *code = KEY_F3; return 0;
            case 'S': *code = KEY_F4; return 0;
        }
        return -1;
    }
    return -1;
}

static void draw_callback(const char *data, int len, void *user) {
    int *fd = (int *)user;
    write(*fd, data, len);
}

int relay_run(const char *socket_path, const char *request_json) {
    if (!relay_setup()) return 1;

    FillyClient *client = filly_client_connect(socket_path);
    if (!client) {
        relay_teardown();
        return 1;
    }

    filly_client_set_draw_callback(client, draw_callback, &tty_fd);

    if (filly_client_send_request(client, request_json) < 0) {
        filly_client_disconnect(client);
        relay_teardown();
        return 1;
    }

    int client_fd = filly_client_get_fd(client);

    while (1) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(client_fd, &fds);
        FD_SET(tty_fd, &fds);
        int maxfd = client_fd > tty_fd ? client_fd : tty_fd;
        struct timeval tv = {0, 100000};
        int sel_ret = select(maxfd + 1, &fds, NULL, NULL, &tv);
        if (sel_ret < 0) break;

        if (FD_ISSET(client_fd, &fds)) {
            int ret = filly_client_process(client);
            if (ret < 0) break;
            if (filly_client_has_response(client)) {
                cJSON *result = NULL;
                bool cancelled = false;
                if (filly_client_get_response(client, &result, &cancelled) == 0) {
                    char *out = cJSON_PrintUnformatted(result);
                    printf("%s\n", out);
                    free(out);
                }
                filly_client_disconnect(client);
                relay_teardown();
                return 0;
            }
        }

        if (FD_ISSET(tty_fd, &fds)) {
            unsigned char c;
            if (read(tty_fd, &c, 1) == 1) {
                if (c == '\033') {
                    int code;
                    if (parse_csi(tty_fd, &code) == 0) {
                        filly_client_send_key(client, code, 0);
                    } else {
                        filly_client_send_key(client, KEY_ESC, 0);
                    }
                } else {
                    if (c == '\t') {
                        filly_client_send_key(client, KEY_TAB, 0);
                    } else if (c == '\r' || c == '\n') {
                        filly_client_send_key(client, KEY_ENTER, 0);
                    } else if (c == 127) {
                        filly_client_send_key(client, KEY_BACKSPACE, 0);
                    } else {
                        filly_client_send_key(client, KEY_CHAR, (char)c);
                    }
                }
            }
        }
    }

    filly_client_disconnect(client);
    relay_teardown();
    return 1;
}