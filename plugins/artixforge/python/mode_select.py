import gi
gi.require_version('Gtk', '4.0')
gi.require_version('Adw', '1')
import os
from gi.repository import Gtk, Adw
from .installation import InstallationWizard
from .poweruser import PowerUserWizard
from .recovery import RecoveryWizard
from .iso import ISOWizard
from .migration import MigrationWizard
from .resume import ResumeWizard

class ModeCard(Gtk.FlowBoxChild):
    def __init__(self, title, description, icon_name):
        super().__init__()
        box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=10)
        box.set_margin_top(20)
        box.set_margin_bottom(20)
        box.set_margin_start(20)
        box.set_margin_end(20)
        icon = Gtk.Image.new_from_icon_name(icon_name)
        icon.set_pixel_size(64)
        box.append(icon)
        label = Gtk.Label(label=title)
        label.add_css_class("title-3")
        box.append(label)
        desc = Gtk.Label(label=description)
        desc.add_css_class("caption")
        desc.set_wrap(True)
        desc.set_max_width_chars(25)
        box.append(desc)
        self.set_child(box)

class ModeSelectPage(Gtk.Box):
    def __init__(self, app):
        super().__init__(orientation=Gtk.Orientation.VERTICAL)
        self.app = app
        label = Gtk.Label()
        label.set_markup('<span size="large" weight="bold">Choose installation mode:</span>')
        label.set_margin_top(20)
        self.append(label)

        flowbox = Gtk.FlowBox()
        flowbox.set_max_children_per_line(3)
        flowbox.set_selection_mode(Gtk.SelectionMode.SINGLE)

        modes = [
            ("Installation", "Guided setup with full configuration flow", "system-run-symbolic"),
            ("Power User", "Source-based package compilation", "applications-engineering-symbolic"),
            ("Recovery", "Scan /mnt for existing system", "tools-check-spelling-symbolic"),
            ("Build ISO", "Create a custom live ISO", "media-optical-symbolic"),
            ("System Migration", "Convert init, desktop, or Arch → Artix", "emblem-synchronizing-symbolic"),
            ("Resume", "Continue interrupted installation", "media-seek-forward-symbolic"),
        ]

        for title, desc, icon in modes:
            flowbox.append(ModeCard(title, desc, icon))

        flowbox.connect("child-activated", self._on_mode_selected)
        self.append(flowbox)

    def _on_mode_selected(self, flowbox, child):
        idx = child.get_index()
        titles = ["Installation", "Power User", "Recovery", "Build ISO", "System Migration", "Resume"]
        chosen = titles[idx]

        if chosen not in ["Resume", "Recovery", "System Migration"]:
            if os.path.exists(self.app.state_file):
                os.remove(self.app.state_file)
            self.app.state.clear()

        self.app.state['MODE'] = chosen.lower().replace(" ", "")
        self.app.state['GUI_MODE'] = 'yes'

        if chosen == "Installation":
            InstallationWizard(self.app).push_pages()
        elif chosen == "Power User":
            PowerUserWizard(self.app).push_pages()
        elif chosen == "Recovery":
            RecoveryWizard(self.app).push_pages()
        elif chosen == "Build ISO":
            ISOWizard(self.app).push_pages()
        elif chosen == "System Migration":
            MigrationWizard(self.app).push_pages()
        elif chosen == "Resume":
            ResumeWizard(self.app).push_pages()