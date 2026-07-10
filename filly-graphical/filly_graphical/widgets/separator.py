from .base import BaseWindow
from gi.repository import Gtk

class SeparatorWindow(BaseWindow):
    def run(self):
        orientation = self.params.get("orientation", "horizontal")
        vbox = self._create_window(400, 50)
        if orientation == "vertical":
            sep = Gtk.Separator(orientation=Gtk.Orientation.VERTICAL)
        else:
            sep = Gtk.Separator(orientation=Gtk.Orientation.HORIZONTAL)
        sep.set_hexpand(True)
        vbox.append(sep)
        self.window.show()
        self.result = None
        self._quit()
        return {"result": self.result, "cancelled": self.cancelled}