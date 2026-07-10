from .base import BaseWindow
from gi.repository import Gtk

class ToggleWindow(BaseWindow):
    def run(self):
        label = self.params.get("label", "Toggle")
        default = self.params.get("default", False)
        vbox = self._create_window(400, 200)
        self._switch = Gtk.Switch()
        self._switch.set_active(default)
        self._switch.set_halign(Gtk.Align.CENTER)
        lbl = Gtk.Label(label=label)
        lbl.set_halign(Gtk.Align.CENTER)
        vbox.append(lbl)
        vbox.append(self._switch)
        vbox.append(self._make_button_box(
            ("OK", lambda b: self._ok(), True),
            ("Cancel", lambda b: self._cancel(), False),
        ))
        return super().run()

    def _ok(self):
        self.result = self._switch.get_active()
        self._quit()

    def _cancel(self):
        self.cancelled = True
        self._quit()