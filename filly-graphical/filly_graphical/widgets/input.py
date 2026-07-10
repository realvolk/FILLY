from .base import BaseWindow
from gi.repository import Gtk

class InputWindow(BaseWindow):
    def run(self):
        vbox = self._create_window(500, 200)
        self.entry = Gtk.Entry()
        self.entry.set_text(self.params.get("default", ""))
        if self.params.get("placeholder"):
            self.entry.set_placeholder_text(self.params["placeholder"])
        self.entry.connect("activate", lambda e: self._submit())
        vbox.append(self.entry)
        vbox.append(self._make_button_box(
            ("OK", lambda b: self._submit(), True),
            ("Cancel", lambda b: self._cancel(), False),
        ))
        self.window.show()
        self.entry.grab_focus()
        loop = GLib.MainLoop()
        self.window.connect("destroy", lambda *_: loop.quit())
        loop.run()
        return {"result": self.result or "", "cancelled": self.cancelled}

    def _submit(self):
        self.result = self.entry.get_text()
        self._quit()

    def _cancel(self):
        self.cancelled = True
        self._quit()