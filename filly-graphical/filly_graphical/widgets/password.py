from .input import InputWindow
from gi.repository import Gtk

class PasswordWindow(InputWindow):
    def run(self):
        vbox = self._create_window(500, 200)
        self.entry = Gtk.PasswordEntry()
        self.entry.set_show_peek_icon(False)
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