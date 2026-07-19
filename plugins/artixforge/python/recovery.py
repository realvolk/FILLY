# plugins/artixforge/python/recovery.py
import gi, subprocess
gi.require_version('Gtk', '4.0')
gi.require_version('Adw', '1')
from gi.repository import Gtk, Adw
from .hub import HubPage

class RecoveryWizard:
    def __init__(self, app):
        self.app = app

    def push_pages(self):
        hub = self._build_hub()
        self.app.nav_view.push(Adw.NavigationPage(child=hub, title="Recovery"))

    def _build_hub(self):
        cats = [
            ("System", [
                {"id":"RECOVERY_STATUS","label":"Install stage","value":self.app.state.get("RECOVERY_STATUS","unknown"),"widget":"input"},
                {"id":"DISK","label":"Disk","value":self.app.state.get("DISK",""),"widget":"input"},
                {"id":"INIT","label":"Init","value":self.app.state.get("INIT","openrc"),"widget":"input"},
                {"id":"FS_TYPE","label":"Filesystem","value":self.app.state.get("FS_TYPE","ext4"),"widget":"input"},
            ]),
            ("Boot", [
                {"id":"BOOTLOADER","label":"Bootloader","value":self.app.state.get("BOOTLOADER",""),"widget":"input"},
                {"id":"KERNEL_CHOICE","label":"Kernel","value":self.app.state.get("KERNEL_CHOICE",""),"widget":"input"},
            ]),
            ("Packages", [
                {"id":"PACMAN_ISSUES","label":"Pacman DB","value":self.app.state.get("PACMAN_ISSUES","none"),"widget":"input"},
            ]),
        ]
        actions = [
            ("Repair Detected Issues", self._on_repair),
            ("Fix Everything", self._on_fix_everything),
        ]
        return HubPage(self.app, cats, actions)

    def _on_repair(self):
        self.app.state['RECOVERY_ACTION'] = "Repair detected issues"
        self.app.state['MODE'] = 'recovery'
        self.app.state['GUI_MODE'] = 'yes'
        self.app.save_state()
        self.app.start_installation("Recovery Mode")

    def _on_fix_everything(self):
        self.app.state['RECOVERY_ACTION'] = "Fix everything"
        self.app.state['MODE'] = 'recovery'
        self.app.state['GUI_MODE'] = 'yes'
        self.app.save_state()
        self.app.start_installation("Recovery Mode")