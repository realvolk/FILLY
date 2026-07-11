import gi
gi.require_version('Gtk', '4.0')
from gi.repository import Gtk, GLib
from filly_graphical.widgets.base import BaseWindow

class CflagsWindow(BaseWindow):
    def run(self):
        self.result = None
        self.cancelled = False

        self._window = Gtk.Window(title=self.title_text or "CFLAGS")
        self._window.set_default_size(500, 350)
        self._window.connect("destroy", lambda w: self._quit())

        vbox = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=10)
        vbox.set_margin_top(20); vbox.set_margin_bottom(20)
        vbox.set_margin_start(20); vbox.set_margin_end(20)
        self._window.set_child(vbox)

        title_label = Gtk.Label(label=self.title_text)
        title_label.add_css_class("title-1")
        vbox.append(title_label)

        grid = Gtk.Grid()
        grid.set_row_spacing(10)
        grid.set_column_spacing(10)
        vbox.append(grid)

        fields = [
            ("CFLAGS", self.params.get("CFLAGS", "")),
            ("CXXFLAGS", self.params.get("CXXFLAGS", "")),
            ("MAKEOPTS", self.params.get("MAKEOPTS", "")),
            ("RUSTFLAGS", self.params.get("RUSTFLAGS", "")),
        ]

        self._entries = {}
        for i, (label, default) in enumerate(fields):
            lbl = Gtk.Label(label=label, xalign=1)
            grid.attach(lbl, 0, i, 1, 1)

            entry = Gtk.Entry()
            entry.set_text(default)
            entry.set_hexpand(True)
            grid.attach(entry, 1, i, 1, 1)
            self._entries[label] = entry

        btn_box = Gtk.Box(spacing=10)
        btn_box.set_halign(Gtk.Align.CENTER)
        save_btn = Gtk.Button(label="Save")
        save_btn.add_css_class("suggested-action")
        save_btn.connect("clicked", self._on_save)
        btn_box.append(save_btn)
        cancel_btn = Gtk.Button(label="Cancel")
        cancel_btn.connect("clicked", self._on_cancel)
        btn_box.append(cancel_btn)
        vbox.append(btn_box)

        self._window.show()
        loop = GLib.MainLoop()
        self._window.connect("destroy", lambda *_: loop.quit())
        loop.run()
        return {"result": self.result, "cancelled": self.cancelled}

    def _on_save(self, btn):
        self.result = {
            "CFLAGS": self._entries["CFLAGS"].get_text(),
            "CXXFLAGS": self._entries["CXXFLAGS"].get_text(),
            "MAKEOPTS": self._entries["MAKEOPTS"].get_text(),
            "RUSTFLAGS": self._entries["RUSTFLAGS"].get_text(),
        }
        self._quit()

    def _on_cancel(self, btn=None):
        self.cancelled = True
        self._quit()