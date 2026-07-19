import gi, os, urllib.request
gi.require_version('Gtk', '4.0')
gi.require_version('Adw', '1')
from gi.repository import Gtk, Adw
from .hub import HubPage

RECIPES_REPO = "https://raw.githubusercontent.com/realvolk/ArtixForge-recipes/main"
LIST_URL = f"{RECIPES_REPO}/.LIST"

class PowerUserWizard:
    def __init__(self, app):
        self.app = app
        self.recipes = []
        self._fetch_recipes()

    def _fetch_recipes(self):
        try:
            with urllib.request.urlopen(LIST_URL) as resp:
                for line in resp.read().decode().strip().split('\n'):
                    if '|' in line:
                        parts = line.split('|', 2)
                        if len(parts) >= 3:
                            self.recipes.append((parts[0], parts[1], parts[2]))
        except:
            self.recipes = [("linux", "OFFICIAL/Base", "Kernel")]

    def push_pages(self):
        hub = self._build_hub()
        self.app.nav_view.push(Adw.NavigationPage(child=hub, title="Power User Configuration"))

    def _build_hub(self):
        cats = [
            ("Profile", [
                {"id":"POWERUSER_PROFILE","label":"Profile","value":"default","widget":"menu","choices":["default","safe","performance","hardened"]},
                {"id":"ARTIX_CFLAGS","label":"CFLAGS","value":"-march=native -O2 -pipe","widget":"input"},
                {"id":"ARTIX_CXXFLAGS","label":"CXXFLAGS","value":"-march=native -O2 -pipe","widget":"input"},
                {"id":"ARTIX_LDFLAGS","label":"LDFLAGS","value":"","widget":"input"},
                {"id":"ARTIX_MAKEFLAGS","label":"MAKEFLAGS","value":"-j$(nproc)","widget":"input"},
            ]),
            ("Init", [
                {"id":"INIT","label":"Init system","value":"openrc","widget":"menu","choices":["openrc","runit","dinit","s6","busybox"]},
            ]),
            ("Packages", [
                {"id":"POWERUSER_PACKAGES","label":"Source packages","value":"","widget":"multiselect","choices":[r[0] for r in self.recipes]},
            ]),
            ("Coreutils", [
                {"id":"COREUTILS","label":"Implementation","value":"gnu","widget":"menu","choices":["gnu","busybox","uutils","artix","custom","none"]},
            ]),
            ("Fallback", [
                {"id":"KEEP_BINARY_KERNEL","label":"Keep binary kernel","value":"yes","widget":"yesno"},
            ]),
        ]
        actions = [("Build", self._on_build)]
        return HubPage(self.app, cats, actions)

    def _on_build(self):
        self.app.state['POWER_USER'] = "yes"
        self.app.state['GUI_MODE'] = "yes"
        self.app.save_state()
        self.app.start_installation("Power User Installation")