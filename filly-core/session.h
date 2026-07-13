#pragma once
#include "widget.h"
#include "backend.h"
#include "../filly-protocol/protocol.h"

WidgetResponse session_run(Widget *w, Backend *backend);