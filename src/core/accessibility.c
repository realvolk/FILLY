#include "accessibility.h"

#ifdef FILLY_ACCESSIBILITY

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <pthread.h>

static int atspi_socket = -1;
static pthread_mutex_t atspi_lock = PTHREAD_MUTEX_INITIALIZER;
static char *last_focused = NULL;

bool accessibility_init(void) {
    const char *bus_path = getenv("AT_SPI_BUS");
    if (!bus_path) bus_path = "/run/at-spi/bus";

    atspi_socket = socket(AF_UNIX, SOCK_STREAM, 0);
    if (atspi_socket < 0) return false;

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", bus_path);

    if (connect(atspi_socket, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(atspi_socket);
        atspi_socket = -1;
        return false;
    }

    return true;
}

void accessibility_shutdown(void) {
    pthread_mutex_lock(&atspi_lock);
    if (atspi_socket >= 0) {
        close(atspi_socket);
        atspi_socket = -1;
    }
    free(last_focused);
    last_focused = NULL;
    pthread_mutex_unlock(&atspi_lock);
}

static void atspi_send(const char *msg) {
    if (atspi_socket < 0 || !msg) return;
    write(atspi_socket, msg, strlen(msg));
    write(atspi_socket, "\n", 1);
}

static void push_node(RenderTree *node, int depth) {
    if (!node) return;
    if (!node->accessible.role || !node->accessible.role[0]) {
        if (node->type == RNODE_CONTAINER && node->u.container.children) {
            for (int i = 0; i < node->u.container.child_count; i++)
                push_node(&node->u.container.children[i], depth);
        }
        if (node->type == RNODE_FLEX && node->u.flex.children) {
            for (int i = 0; i < node->u.flex.child_count; i++)
                push_node(&node->u.flex.children[i], depth);
        }
        if (node->type == RNODE_GRID && node->u.grid.children) {
            for (int i = 0; i < node->u.grid.child_count; i++)
                push_node(&node->u.grid.children[i], depth);
        }
        return;
    }

    char msg[1024];
    const char *label = node->accessible.label ? node->accessible.label : "";
    const char *role = node->accessible.role;

    snprintf(msg, sizeof(msg),
        "{\"type\":\"object\",\"role\":\"%s\",\"name\":\"%s\",\"depth\":%d,"
        "\"x\":%d,\"y\":%d,\"w\":%d,\"h\":%d}",
        role, label, depth,
        node->rect.x, node->rect.y, node->rect.w, node->rect.h);
    atspi_send(msg);

    if (node->type == RNODE_CONTAINER && node->u.container.children) {
        for (int i = 0; i < node->u.container.child_count; i++)
            push_node(&node->u.container.children[i], depth + 1);
    }
    if (node->type == RNODE_FLEX && node->u.flex.children) {
        for (int i = 0; i < node->u.flex.child_count; i++)
            push_node(&node->u.flex.children[i], depth + 1);
    }
    if (node->type == RNODE_GRID && node->u.grid.children) {
        for (int i = 0; i < node->u.grid.child_count; i++)
            push_node(&node->u.grid.children[i], depth + 1);
    }
    if (node->type == RNODE_TABS && node->u.tabs.child)
        push_node(node->u.tabs.child, depth + 1);
    if (node->type == RNODE_SPLIT_PANES) {
        if (node->u.split_panes.first) push_node(node->u.split_panes.first, depth + 1);
        if (node->u.split_panes.second) push_node(node->u.split_panes.second, depth + 1);
        if (node->u.split_panes.third) push_node(node->u.split_panes.third, depth + 1);
    }
}

void accessibility_push_tree(RenderTree *tree, const char *focused_id) {
    pthread_mutex_lock(&atspi_lock);
    if (atspi_socket < 0) {
        pthread_mutex_unlock(&atspi_lock);
        return;
    }

    atspi_send("{\"type\":\"frame_start\"}");

    push_node(tree, 0);

    if (focused_id && focused_id[0]) {
        if (!last_focused || strcmp(last_focused, focused_id) != 0) {
            char focus_msg[512];
            snprintf(focus_msg, sizeof(focus_msg),
                "{\"type\":\"focus\",\"id\":\"%s\"}", focused_id);
            atspi_send(focus_msg);

            free(last_focused);
            last_focused = strdup(focused_id);
        }
    }

    atspi_send("{\"type\":\"frame_end\"}");
    pthread_mutex_unlock(&atspi_lock);
}

#endif