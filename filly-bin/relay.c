#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <signal.h>
#include "../filly-core/event.h"

static struct termios orig;

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

static void send_key(int sock_fd, KeyCode code, char ch) {
    char header[32];
    int hl = snprintf(header, sizeof(header), "KEY %d %c\n", (int)code, (int)ch);
    write(sock_fd, header, hl);
}

static void parse_and_send_key(int tty_fd, int sock_fd) {
    int c = read_byte_timeout(tty_fd, 100);
    if (c < 0) return;

    if (c != '\033') {
        switch (c) {
            case '\t':  send_key(sock_fd, KEY_TAB, '\t'); return;
            case '\r':
            case '\n':  send_key(sock_fd, KEY_ENTER, '\r'); return;
            case 127:   send_key(sock_fd, KEY_BACKSPACE, 127); return;
            default:    send_key(sock_fd, KEY_CHAR, (char)c); return;
        }
    }

    int c2 = read_byte_timeout(tty_fd, 10);
    if (c2 < 0) { send_key(sock_fd, KEY_ESC, 0); return; }

    if (c2 == '[') {
        int c3 = read_byte_timeout(tty_fd, 20);
        if (c3 < 0) { send_key(sock_fd, KEY_ESC, 0); return; }

        if (c3 == '[') {
            int c4 = read_byte_timeout(tty_fd, 20);
            if (c4 < 0) { send_key(sock_fd, KEY_ESC, 0); return; }
            KeyCode code = KEY_NULL;
            switch (c4) {
                case 'A': code = KEY_F1; break;
                case 'B': code = KEY_F2; break;
                case 'C': code = KEY_F3; break;
                case 'D': code = KEY_F4; break;
                case 'E': code = KEY_F5; break;
            }
            if (code != KEY_NULL) send_key(sock_fd, code, 0);
            return;
        }

        char params[16] = {0};
        int pi = 0;

        while (pi < 15) {
            if ((c3 >= '0' && c3 <= '9') || c3 == ';') {
                if (pi < 15) params[pi++] = (char)c3;
            } else if ((c3 >= 'A' && c3 <= 'Z') || (c3 >= 'a' && c3 <= 'z') || c3 == '~') {
                params[pi] = '\0';
                KeyCode code = KEY_NULL;
                if (pi == 0) {
                    switch (c3) {
                        case 'A': code = KEY_UP; break;
                        case 'B': code = KEY_DOWN; break;
                        case 'C': code = KEY_RIGHT; break;
                        case 'D': code = KEY_LEFT; break;
                        case 'H': code = KEY_HOME; break;
                        case 'F': code = KEY_END; break;
                        case 'Z': code = KEY_BACKTAB; break;
                    }
                } else {
                    int p1 = atoi(params);
                    if (c3 == '~') {
                        switch (p1) {
                            case 1: case 7: code = KEY_HOME; break;
                            case 2: code = KEY_INSERT; break;
                            case 3: code = KEY_DELETE; break;
                            case 4: case 8: code = KEY_END; break;
                            case 5: code = KEY_PAGEUP; break;
                            case 6: code = KEY_PAGEDOWN; break;
                            case 11: code = KEY_F1; break;
                            case 12: code = KEY_F2; break;
                            case 13: code = KEY_F3; break;
                            case 14: code = KEY_F4; break;
                            case 15: code = KEY_F5; break;
                            case 17: code = KEY_F6; break;
                            case 18: code = KEY_F7; break;
                            case 19: code = KEY_F8; break;
                            case 20: code = KEY_F9; break;
                            case 21: code = KEY_F10; break;
                            case 23: code = KEY_F11; break;
                            case 24: code = KEY_F12; break;
                            default: break;
                        }
                    }
                }
                if (code != KEY_NULL) send_key(sock_fd, code, 0);
                return;
            }
            c3 = read_byte_timeout(tty_fd, 20);
            if (c3 < 0) break;
        }
        return;
    }

    if (c2 == 'O') {
        int c3 = read_byte_timeout(tty_fd, 10);
        if (c3 < 0) { send_key(sock_fd, KEY_ESC, 0); return; }
        KeyCode code = KEY_NULL;
        switch (c3) {
            case 'P': code = KEY_F1; break;
            case 'Q': code = KEY_F2; break;
            case 'R': code = KEY_F3; break;
            case 'S': code = KEY_F4; break;
        }
        if (code != KEY_NULL) send_key(sock_fd, code, 0);
        return;
    }

    send_key(sock_fd, KEY_ESC, 0);
}

int relay_main(const char *sock_path) {
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    struct sockaddr_un addr = { .sun_family = AF_UNIX };
    strncpy(addr.sun_path, sock_path, sizeof(addr.sun_path) - 1);
    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("connect"); return 1;
    }

    int tty_fd = open("/dev/tty", O_RDWR);
    if (tty_fd < 0) return 1;

    struct winsize ws;
    if (ioctl(tty_fd, TIOCGWINSZ, &ws) == 0) {
        char size_buf[32];
        int sl = snprintf(size_buf, sizeof(size_buf), "SIZE %d %d\n", ws.ws_col, ws.ws_row);
        write(fd, size_buf, sl);
    }

    char json[524288];
    int n = read(STDIN_FILENO, json, sizeof(json) - 1);
    if (n <= 0) return 1;
    json[n] = '\0';
    write(fd, json, n);
    write(fd, "\n", 1);

    tcgetattr(tty_fd, &orig);
    struct termios raw = orig;
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_iflag &= ~(IXON | ICRNL | BRKINT | INPCK | ISTRIP);
    raw.c_cflag |= CS8;
    raw.c_oflag &= ~OPOST;
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;
    tcsetattr(tty_fd, TCSAFLUSH, &raw);

    char buf[524288];
    while (1) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(tty_fd, &rfds);
        FD_SET(fd, &rfds);
        int maxfd = tty_fd > fd ? tty_fd : fd;
        select(maxfd + 1, &rfds, NULL, NULL, NULL);

        if (FD_ISSET(tty_fd, &rfds)) {
            parse_and_send_key(tty_fd, fd);
        }
        if (FD_ISSET(fd, &rfds)) {
            char line[256];
            int i = 0;
            while (i < (int)sizeof(line) - 1) {
                if (read(fd, line + i, 1) <= 0) goto done;
                if (line[i] == '\n') { line[i] = '\0'; break; }
                i++;
            }
            if (strncmp(line, "DRAW ", 5) == 0) {
                int len;
                sscanf(line + 5, "%d", &len);
                int total = 0;
                while (total < len) {
                    int r = read(fd, buf + total, len - total);
                    if (r <= 0) goto done;
                    total += r;
                }
                write(tty_fd, buf, len);
                read(fd, buf, 1);
            } else if (strncmp(line, "SIZE ", 5) == 0) {
            } else {
                write(STDOUT_FILENO, line, strlen(line));
                write(STDOUT_FILENO, "\n", 1);
                goto done;
            }
        }
    }
done:
    tcsetattr(tty_fd, TCSAFLUSH, &orig);
    return 0;
}