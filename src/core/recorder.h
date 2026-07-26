#pragma once
#include "core/backend.h"
#include "core/widget.h"
#include "protocol/protocol.h"
#include <stdbool.h>

void recorder_start(void);
void recorder_stop(void);
void recorder_record_frame(const char *widget_type, const char *json_params, WidgetResponse resp, const char *ansi_buf, int ansi_len);
void recorder_record_event(Event ev);
bool recorder_save(const char *path);
bool recorder_load(const char *path);
Event recorder_next_event(void);
bool recorder_has_events(void);
void recorder_reset(void);