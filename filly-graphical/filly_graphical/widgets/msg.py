from .base import BaseWindow
from gi.repository import Gtk

class MsgWindow(BaseWindow):
    def run(self):
        vbox = self._create_window(400, 200)
        btn = Gtk.Button(label="OK")
        btn.add_css_class("suggested-action")
        btn.set_halign(Gtk.Align.CENTER)
        btn.connect("clicked", lambda b: self._quit())
        vbox.append(btn)
        return super().run()