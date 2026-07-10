from .base import BaseWindow
from gi.repository import Gtk, GLib

class TooltipWindow(BaseWindow):
    def run(self):
        text = self.params.get("text", "")

        self._window = Gtk.Window(title="")
        self._window.set_default_size(200, 50)
        self._window.set_decorated(False)
        self._window.set_keep_above(True)
        self._window.set_accessible_role("tooltip")

        label = Gtk.Label(label=text)
        label.set_wrap(True)
        label.set_margin_top(5); label.set_margin_bottom(5)
        label.set_margin_start(10); label.set_margin_end(10)
        label.set_accessible_label(f"Tooltip: {text}")
        self._window.set_child(label)

        GLib.timeout_add_seconds(3, self._quit)
        self.result = None
        self.cancelled = False

        self._window.show()
        loop = GLib.MainLoop()
        self._window.connect("destroy", lambda *_: loop.quit())
        loop.run()
        return {"result": self.result, "cancelled": self.cancelled}