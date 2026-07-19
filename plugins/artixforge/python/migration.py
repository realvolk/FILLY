import gi, os
gi.require_version('Gtk', '4.0')
gi.require_version('Adw', '1')
from gi.repository import Gtk, Adw
from .hub import HubPage

class MigrationWizard:
    def __init__(self, app):
        self.app = app

    def push_pages(self):
        hub = self._build_hub()
        self.app.nav_view.push(Adw.NavigationPage(child=hub, title="System Migration"))

    def _build_hub(self):
        inits = ["openrc", "runit", "dinit", "s6", "systemd"]
        des = ["kde", "sonicde", "xfce", "lxqt", "lxde", "hyprland", "sway", "niri", "i3wm", "dwm", "vxwm", "icewm", "mango", "cinnamon", "budgie", "moksha", "cosmic", "none"]
        cats = [
            ("Init Migration", [
                {"id":"MIGRATION_SRC","label":"Source Init","value":self.app.state.get("DETECTED_INIT","openrc"),"widget":"menu","choices":inits},
                {"id":"MIGRATION_TGT","label":"Target Init","value":"dinit","widget":"menu","choices":["openrc","runit","dinit","s6"]},
            ]),
            ("Desktop Migration", [
                {"id":"MIGRATION_SRC_DE","label":"Source Desktop","value":self.app.state.get("DETECTED_DE","none"),"widget":"menu","choices":des},
                {"id":"MIGRATION_TGT_DE","label":"Target Desktop","value":"none","widget":"menu","choices":des},
            ]),
            ("Arch to Artix", [
                {"id":"INIT","label":"Target init","value":"dinit","widget":"menu","choices":["openrc","runit","dinit","s6"]},
                {"id":"ENABLE_ARCH_REPOS","label":"Keep Arch repos","value":"yes","widget":"yesno"},
            ]),
        ]
        actions = [
            ("Migrate Init", self._on_init_migrate),
            ("Migrate Desktop", self._on_desktop_migrate),
            ("Arch to Artix", self._on_ata_migrate),
        ]
        return HubPage(self.app, cats, actions)

    def _on_init_migrate(self):
        self.app.state['MIGRATION_TYPE'] = 'init'
        self.app.state['GUI_MODE'] = 'yes'
        self.app.state['MODE'] = 'migrate'
        self.app.save_state()
        self.app.start_installation("Init Migration")

    def _on_desktop_migrate(self):
        self.app.state['MIGRATION_TYPE'] = 'desktop'
        self.app.state['GUI_MODE'] = 'yes'
        self.app.state['MODE'] = 'migrate'
        self.app.save_state()
        self.app.start_installation("Desktop Migration")

    def _on_ata_migrate(self):
        self.app.state['MIGRATION_TYPE'] = 'ata'
        self.app.state['MIGRATION_SRC'] = 'arch'
        self.app.state['GUI_MODE'] = 'yes'
        self.app.state['MODE'] = 'migrate'
        self.app.save_state()
        self.app.start_installation("Arch to Artix Migration")