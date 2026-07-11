from filly_graphical.backend import GuiBackend
from filly_graphical.widgets.hub import HubWindow

def register():
    GuiBackend.register_widget("install_hub", HubWindow)
    GuiBackend.register_widget("recovery", HubWindow)
    GuiBackend.register_widget("iso", HubWindow)
    GuiBackend.register_widget("migration_init", HubWindow)
    GuiBackend.register_widget("migration_desktop", HubWindow)
    GuiBackend.register_widget("poweruser", HubWindow)