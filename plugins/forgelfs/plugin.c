#include "core/widget.h"

extern Widget *forge_hub_factory(const WidgetRequest *req);
extern Widget *forge_anvil_factory(const WidgetRequest *req);

void register_plugins(void (*reg)(const char *, WidgetFactory)) {
    reg("forge_hub", forge_hub_factory);
    reg("forge_anvil", forge_anvil_factory);
}