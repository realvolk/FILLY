from filly_graphical.backend import GuiBackend
from filly_graphical.widgets.hub import HubWindow

def register():
    GuiBackend.register_widget("install_hub", HubWindow)
    GuiBackend.register_widget("recovery", HubWindow)
    GuiBackend.register_widget("iso", HubWindow)
    GuiBackend.register_widget("migration_init", HubWindow)
    GuiBackend.register_widget("migration_desktop", HubWindow)
    GuiBackend.register_widget("poweruser", HubWindow)
    GuiBackend.register_widget("config_mode", ConfigModeLauncher)

class ConfigModeLauncher:
    def __init__(self, params, title_color=212, accent_color=34):
        self.params = params

    def run(self):
        from .app import run_config_mode
        state_file = self.params.get("state_file", "/tmp/artix-installer/state.conf")
        run_config_mode(state_file)
        return {"result": None, "cancelled": False}