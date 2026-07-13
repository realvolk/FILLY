#include "../../filly-core/widget.h"

extern Widget *install_hub_factory(const WidgetRequest *req);
extern Widget *anvil_factory(const WidgetRequest *req);
extern Widget *poweruser_factory(const WidgetRequest *req);
extern Widget *recovery_factory(const WidgetRequest *req);
extern Widget *iso_factory(const WidgetRequest *req);
extern Widget *migration_init_factory(const WidgetRequest *req);
extern Widget *migration_desktop_factory(const WidgetRequest *req);
extern Widget *password_confirm_factory(const WidgetRequest *req);
extern Widget *user_manager_factory(const WidgetRequest *req);

void register_plugins(void (*reg)(const char *, WidgetFactory)) {
    reg("install_hub", install_hub_factory);
    reg("anvil", anvil_factory);
    reg("poweruser", poweruser_factory);
    reg("recovery", recovery_factory);
    reg("iso", iso_factory);
    reg("migration_init", migration_init_factory);
    reg("migration_desktop", migration_desktop_factory);
    reg("password_confirm", password_confirm_factory);
    reg("user_manager", user_manager_factory);
}