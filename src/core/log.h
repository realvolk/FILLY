#pragma once

typedef enum { LOG_DEBUG, LOG_INFO, LOG_WARN, LOG_ERROR, LOG_FATAL } LogLevel;

void log_init(const char *path, LogLevel min_level);
void log_write(LogLevel level, const char *file, int line, const char *fmt, ...);

#define LOG_DEBUG(...) log_write(LOG_DEBUG, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_INFO(...)  log_write(LOG_INFO,  __FILE__, __LINE__, __VA_ARGS__)
#define LOG_WARN(...)  log_write(LOG_WARN,  __FILE__, __LINE__, __VA_ARGS__)
#define LOG_ERROR(...) log_write(LOG_ERROR, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_FATAL(...) log_write(LOG_FATAL, __FILE__, __LINE__, __VA_ARGS__)