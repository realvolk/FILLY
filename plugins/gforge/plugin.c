#include "../../filly-core/widget.h"

extern Widget *stage3_factory(const WidgetRequest *req);
extern Widget *profile_factory(const WidgetRequest *req);
extern Widget *kernel_factory(const WidgetRequest *req);
extern Widget *use_flags_factory(const WidgetRequest *req);
extern Widget *cflags_factory(const WidgetRequest *req);

void register_plugins(void (*reg)(const char *, WidgetFactory)) {
    reg("stage3_picker", stage3_factory);
    reg("profile_picker", profile_factory);
    reg("kernel_picker", kernel_factory);
    reg("use_flags", use_flags_factory);
    reg("cflags", cflags_factory);
    reg("portage_licenses", use_flags_factory);
    reg("portage_features", use_flags_factory);
    reg("portage_overlays", use_flags_factory);
    reg("portage_mirrors", cflags_factory);
    reg("portage_accept_keywords", kernel_factory);
    reg("portage_video_cards", kernel_factory);
    reg("portage_binhost", cflags_factory);
    reg("portage_desktop_extras", use_flags_factory);
    reg("portage_tool_groups", use_flags_factory);
}