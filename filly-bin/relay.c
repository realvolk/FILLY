#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>
#include <signal.h>

static struct termios orig;

static void restore_term(void) { tcsetattr(0, TCSAFLUSH, &orig); }

static void set_raw(void) {
    tcgetattr(0, &orig);
    struct termios raw = orig;
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_iflag &= ~(IXON | ICRNL | BRKINT | INPCK | ISTRIP);
    raw.c_cflag |= CS8;
    raw.c_oflag &= ~OPOST;
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;
    tcsetattr(0, TCSAFLUSH, &raw);
}

int relay_main(const char *sock_path) {
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    struct sockaddr_un addr = { .sun_family = AF_UNIX };
    strncpy(addr.sun_path, sock_path, sizeof(addr.sun_path)-1);
    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("connect"); return 1;
    }

    int tty_fd = open("/dev/tty", O_RDWR);
    if (tty_fd < 0) return 1;

    char json[524288];
    int n = read(0, json, sizeof(json)-1);
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
        fd_set rfds; FD_ZERO(&rfds);
        FD_SET(tty_fd, &rfds); FD_SET(fd, &rfds);
        int maxfd = tty_fd > fd ? tty_fd : fd;
        select(maxfd+1, &rfds, NULL, NULL, NULL);

        if (FD_ISSET(tty_fd, &rfds)) {
            char ch;
            if (read(tty_fd, &ch, 1) <= 0) break;
            char header[32];
            int hl = snprintf(header, sizeof(header), "KEY %d %c\n", (int)ch, ch);
            write(fd, header, hl);
        }
        if (FD_ISSET(fd, &rfds)) {
            char line[256]; int i = 0;
            while (i < (int)sizeof(line)-1) {
                if (read(fd, line+i, 1) <= 0) goto done;
                if (line[i] == '\n') { line[i] = '\0'; break; }
                i++;
            }
            if (strncmp(line, "DRAW ", 5) == 0) {
                int len; sscanf(line+5, "%d", &len);
                int total = 0;
                while (total < len) {
                    int r = read(fd, buf+total, len-total);
                    if (r <= 0) goto done;
                    total += r;
                }
                write(1, buf, len);
                read(fd, buf, 1);
            } else if (strncmp(line, "SIZE ", 5) == 0) {
                /* ignore */
            } else {
                write(1, line, strlen(line));
                write(1, "\n", 1);
                goto done;
            }
        }
    }
done:
    tcsetattr(tty_fd, TCSAFLUSH, &orig);
    return 0;
}