#include "relay.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>
#include <stdbool.h>
#include "../filly-core/event.h"

static struct termios orig_termios;
static int tty_fd = -1;

static void relay_teardown(void) {
    if (tty_fd >= 0) {
        tcsetattr(tty_fd, TCSAFLUSH, &orig_termios);
        close(tty_fd);
        tty_fd = -1;
    }
}

static bool relay_setup(void) {
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

int relay_run(const char *socket_path, const char *request_json) {
    if (!relay_setup()) { fprintf(stderr, "relay: cannot open /dev/tty\n"); return 1; }
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) { relay_teardown(); return 1; }
    struct sockaddr_un addr = { .sun_family = AF_UNIX };
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);
    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(sock); relay_teardown(); return 1;
    }
    write(sock, request_json, strlen(request_json));
    write(sock, "\n", 1);

    char buf[524288]; int buf_len = 0;
    while (1) {
        fd_set fds; FD_ZERO(&fds); FD_SET(sock, &fds); FD_SET(tty_fd, &fds);
        int maxfd = sock > tty_fd ? sock : tty_fd;
        struct timeval tv = {0, 100000};
        if (select(maxfd + 1, &fds, NULL, NULL, &tv) < 0) break;

        if (FD_ISSET(sock, &fds)) {
            int n = read(sock, buf + buf_len, sizeof(buf) - buf_len - 1);
            if (n <= 0) break;
            buf_len += n; buf[buf_len] = '\0';
            char *p = buf;
            while (1) {
                char *nl = strchr(p, '\n');
                if (!nl) break;
                *nl = '\0';
                cJSON *msg = cJSON_Parse(p);
                if (msg) {
                    cJSON *type = cJSON_GetObjectItem(msg, "type");
                    if (type && type->valuestring) {
                        if (strcmp(type->valuestring, "draw") == 0) {
                            cJSON *len = cJSON_GetObjectItem(msg, "len");
                            if (len) {
                                int draw_len = len->valueint;
                                char *data = nl + 1;
                                int rem = buf + buf_len - data;
                                if (rem >= draw_len + 1) {
                                    write(tty_fd, data, draw_len);
                                    char *after = data + draw_len;
                                    if (*after == '\n') after++;
                                    memmove(p, after, buf + buf_len - after);
                                    buf_len -= (after - p);
                                    buf[buf_len] = '\0';
                                    cJSON_Delete(msg);
                                    continue;
                                }
                            }
                        } else if (strcmp(type->valuestring, "response") == 0) {
                            cJSON *result = cJSON_GetObjectItem(msg, "result");
                            char *out = cJSON_PrintUnformatted(result);
                            printf("%s\n", out);
                            free(out);
                            cJSON_Delete(msg);
                            relay_teardown();
                            close(sock);
                            return 0;
                        }
                    }
                    cJSON_Delete(msg);
                }
                p = nl + 1;
                if (p >= buf + buf_len) { buf_len = 0; buf[0] = '\0'; break; }
            }
            if (p < buf + buf_len) {
                memmove(buf, p, buf + buf_len - p);
                buf_len = buf + buf_len - p;
                buf[buf_len] = '\0';
            }
        }

        if (FD_ISSET(tty_fd, &fds)) {
            unsigned char c;
            if (read(tty_fd, &c, 1) == 1) {
                if (c == '\033') {
                    int code;
                    if (parse_csi(tty_fd, &code) == 0) {
                        char m[128]; snprintf(m, sizeof(m), "{\"type\":\"key\",\"code\":%d,\"ch\":\"\"}\n", code);
                        write(sock, m, strlen(m));
                    } else {
                        char m[64]; snprintf(m, sizeof(m), "{\"type\":\"key\",\"code\":%d,\"ch\":\"\"}\n", KEY_ESC);
                        write(sock, m, strlen(m));
                    }
                } else {
                    char m[128];
                    if (c == '\t') snprintf(m, sizeof(m), "{\"type\":\"key\",\"code\":%d,\"ch\":\"\"}\n", KEY_TAB);
                    else if (c == '\r' || c == '\n') snprintf(m, sizeof(m), "{\"type\":\"key\",\"code\":%d,\"ch\":\"\"}\n", KEY_ENTER);
                    else if (c == 127) snprintf(m, sizeof(m), "{\"type\":\"key\",\"code\":%d,\"ch\":\"\"}\n", KEY_BACKSPACE);
                    else snprintf(m, sizeof(m), "{\"type\":\"key\",\"code\":%d,\"ch\":\"%c\"}\n", KEY_CHAR, c);
                    write(sock, m, strlen(m));
                }
            }
        }
    }
    relay_teardown();
    close(sock);
    return 1;
}