#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>

static FILE *log_file = NULL;
static LogLevel min_level = LOG_INFO;

void log_init(const char *path, LogLevel level) {
    min_level = level;
    log_file = path ? fopen(path, "a") : stderr;
    if (!log_file) log_file = stderr;
}

void log_write(LogLevel level, const char *file, int line, const char *fmt, ...) {
    if (level < min_level || !log_file) return;
    const char *level_str[] = {"DEBUG","INFO","WARN","ERROR","FATAL"};
    time_t now = time(NULL);
    char time_buf[32];
    strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", localtime(&now));
    fprintf(log_file, "[%s] [%s] %s:%d: ", time_buf, level_str[level], file, line);
    va_list ap; va_start(ap, fmt);
    vfprintf(log_file, fmt, ap);
    va_end(ap);
    fprintf(log_file, "\n");
    fflush(log_file);
}