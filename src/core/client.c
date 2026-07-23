#include "client.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>
#include <fcntl.h>
#include <errno.h>

#define CLIENT_BUF_SIZE 524288

struct FillyClient {
    int fd;
    char buf[CLIENT_BUF_SIZE];
    int buf_len;
    bool response_received;
    cJSON *response_result;
    bool response_cancelled;
    FillyDrawCallback draw_cb;
    void *draw_user;
};

FillyClient *filly_client_connect(const char *socket_path) {
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return NULL;

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd);
        return NULL;
    }

    int flags = fcntl(fd, F_GETFL, 0);
    if (flags >= 0) fcntl(fd, F_SETFL, flags | O_NONBLOCK);

    FillyClient *c = calloc(1, sizeof(FillyClient));
    if (!c) {
        close(fd);
        return NULL;
    }
    c->fd = fd;
    c->buf_len = 0;
    c->response_received = false;
    c->response_result = NULL;
    c->response_cancelled = false;
    c->draw_cb = NULL;
    c->draw_user = NULL;
    return c;
}

void filly_client_disconnect(FillyClient *c) {
    if (!c) return;
    if (c->fd >= 0) close(c->fd);
    if (c->response_result) cJSON_Delete(c->response_result);
    free(c);
}

int filly_client_get_fd(FillyClient *c) {
    if (!c) return -1;
    return c->fd;
}

int filly_client_send_request(FillyClient *c, const char *json) {
    if (!c || c->fd < 0 || !json) return -1;
    size_t len = strlen(json);
    if (write(c->fd, json, len) != (ssize_t)len) return -1;
    if (write(c->fd, "\n", 1) != 1) return -1;
    return 0;
}

int filly_client_send_key(FillyClient *c, int keycode, char ch) {
    if (!c || c->fd < 0) return -1;
    char msg[256];
    if (ch == 0) {
        snprintf(msg, sizeof(msg), "{\"type\":\"key\",\"code\":%d,\"ch\":\"\"}\n", keycode);
    } else if (ch == '"') {
        snprintf(msg, sizeof(msg), "{\"type\":\"key\",\"code\":%d,\"ch\":\"\\\"\"}\n", keycode);
    } else if (ch == '\\') {
        snprintf(msg, sizeof(msg), "{\"type\":\"key\",\"code\":%d,\"ch\":\"\\\\\"}\n", keycode);
    } else if (ch >= 32 && ch <= 126) {
        snprintf(msg, sizeof(msg), "{\"type\":\"key\",\"code\":%d,\"ch\":\"%c\"}\n", keycode, ch);
    } else {
        snprintf(msg, sizeof(msg), "{\"type\":\"key\",\"code\":%d,\"ch\":\"\"}\n", keycode);
    }
    if (write(c->fd, msg, strlen(msg)) != (ssize_t)strlen(msg)) return -1;
    return 0;
}

int filly_client_send_quit(FillyClient *c) {
    if (!c || c->fd < 0) return -1;
    const char *msg = "{\"type\":\"quit\"}\n";
    size_t len = strlen(msg);
    if (write(c->fd, msg, len) != (ssize_t)len) return -1;
    return 0;
}

int filly_client_poll(FillyClient *c, int timeout_ms) {
    if (!c || c->fd < 0) return -1;
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(c->fd, &fds);
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    int ret = select(c->fd + 1, &fds, NULL, NULL, timeout_ms >= 0 ? &tv : NULL);
    if (ret < 0) return -1;
    if (ret == 0) return 0;
    return FD_ISSET(c->fd, &fds) ? 1 : 0;
}

static int client_process_buffer(FillyClient *c) {
    char *p = c->buf;
    while (1) {
        char *nl = memchr(p, '\n', c->buf + c->buf_len - p);
        if (!nl) break;
        *nl = '\0';

        cJSON *msg = cJSON_Parse(p);
        if (msg) {
            cJSON *type = cJSON_GetObjectItem(msg, "type");
            if (type && type->valuestring) {
                if (strcmp(type->valuestring, "draw") == 0) {
                    cJSON *len_json = cJSON_GetObjectItem(msg, "len");
                    if (len_json && len_json->valueint > 0) {
                        int draw_len = len_json->valueint;
                        char *data = nl + 1;
                        int remaining = c->buf + c->buf_len - data;
                        if (remaining >= draw_len + 1) {
                            if (c->draw_cb)
                                c->draw_cb(data, draw_len, c->draw_user);
                            char *after = data + draw_len;
                            if (*after == '\n') after++;
                            memmove(p, after, c->buf + c->buf_len - after);
                            c->buf_len -= (after - p);
                            c->buf[c->buf_len] = '\0';
                            cJSON_Delete(msg);
                            continue;
                        } else {
                            cJSON_Delete(msg);
                            *nl = '\n';
                            break;
                        }
                    }
                } else if (strcmp(type->valuestring, "response") == 0) {
                    c->response_received = true;
                    c->response_result = cJSON_DetachItemFromObject(msg, "result");
                    cJSON *canc = cJSON_GetObjectItem(msg, "cancelled");
                    c->response_cancelled = canc && (canc->type == cJSON_True || (canc->type == cJSON_Number && canc->valueint));
                    cJSON_Delete(msg);
                    char *after = nl + 1;
                    memmove(p, after, c->buf + c->buf_len - after);
                    c->buf_len -= (after - p);
                    c->buf[c->buf_len] = '\0';
                    return 1;
                }
            }
            cJSON_Delete(msg);
        }
        p = nl + 1;
        if (p >= c->buf + c->buf_len) {
            c->buf_len = 0;
            c->buf[0] = '\0';
            break;
        }
    }
    if (p < c->buf + c->buf_len) {
        memmove(c->buf, p, c->buf + c->buf_len - p);
        c->buf_len = c->buf + c->buf_len - p;
        c->buf[c->buf_len] = '\0';
    }
    return 0;
}

int filly_client_process(FillyClient *c) {
    if (!c || c->fd < 0) return -1;
    char tmp[65536];
    while (1) {
        ssize_t n = read(c->fd, tmp, sizeof(tmp));
        if (n > 0) {
            if (c->buf_len + n >= CLIENT_BUF_SIZE) return -1;
            memcpy(c->buf + c->buf_len, tmp, n);
            c->buf_len += n;
            c->buf[c->buf_len] = '\0';
        } else if (n == 0) {
            return -1;
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            return -1;
        }
    }
    return client_process_buffer(c);
}

bool filly_client_has_response(FillyClient *c) {
    if (!c) return false;
    return c->response_received;
}

int filly_client_get_response(FillyClient *c, cJSON **result, bool *cancelled) {
    if (!c || c->fd < 0) return -1;

    while (!c->response_received) {
        int poll_ret = filly_client_poll(c, 100);
        if (poll_ret < 0) return -1;
        if (poll_ret == 0) continue;

        int space = CLIENT_BUF_SIZE - c->buf_len - 1;
        if (space <= 0) return -1;
        int n = read(c->fd, c->buf + c->buf_len, space);
        if (n <= 0) return -1;
        c->buf_len += n;
        c->buf[c->buf_len] = '\0';

        int ret = client_process_buffer(c);
        if (ret < 0) return -1;
    }

    if (result) *result = c->response_result;
    else if (c->response_result) cJSON_Delete(c->response_result);
    c->response_result = NULL;
    if (cancelled) *cancelled = c->response_cancelled;
    return 0;
}

void filly_client_set_draw_callback(FillyClient *c, FillyDrawCallback cb, void *user) {
    if (!c) return;
    c->draw_cb = cb;
    c->draw_user = user;
}