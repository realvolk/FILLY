import os
from .base import BaseWindow
from gi.repository import Gtk

class SummaryWindow(BaseWindow):
    def run(self):
        text = self.params.get("message", "")
        file_path = self.params.get("file")
        if file_path:
            try:
                with open(file_path, "r") as f:
                    text = f.read()
            except Exception:
                text = f"[Error reading {file_path}]"

        vbox = self._create_window(650, 500)
        scrolled = Gtk.ScrolledWindow()
        scrolled.set_policy(Gtk.PolicyType.AUTOMATIC, Gtk.PolicyType.AUTOMATIC)
        textview = Gtk.TextView()
        textview.set_editable(False)
        textview.set_monospace(True)
        textview.get_buffer().set_text(text)
        textview.set_wrap_mode(Gtk.WrapMode.WORD)
        scrolled.set_child(textview)
        vbox.append(scrolled)

        btn = Gtk.Button(label="OK")
        btn.add_css_class("suggested-action")
        btn.connect("clicked", lambda b: self._quit())
        vbox.append(btn)
        return super().run()