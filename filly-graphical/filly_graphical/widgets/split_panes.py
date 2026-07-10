import gi
gi.require_version('Gtk', '4.0')
from gi.repository import Gtk, GLib
from .base import BaseWindow

class SplitPanesWindow(BaseWindow):
    def run(self):
        orientation_str = self.params.get("orientation", "horizontal")
        orientation = Gtk.Orientation.HORIZONTAL if orientation_str == "horizontal" else Gtk.Orientation.VERTICAL
        self.result = None
        self.cancelled = False

        self._window = Gtk.Window(title=self.title_text or "Split Panes")
        self._window.set_default_size(700, 500)
        self._window.connect("destroy", lambda w: self._quit())

        vbox = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=0)
        self._window.set_child(vbox)

        title_label = Gtk.Label(label=self.title_text)
        title_label.add_css_class("title-1")
        title_label.set_margin_top(10)
        vbox.append(title_label)

        paned = Gtk.Paned(orientation=orientation)
        paned.set_hexpand(True); paned.set_vexpand(True)
        left_label = Gtk.Label(label="Left pane")
        right_label = Gtk.Label(label="Right pane")
        paned.set_start_child(left_label)
        paned.set_end_child(right_label)
        vbox.append(paned)

        btn_box = Gtk.Box(spacing=10)
        btn_box.set_halign(Gtk.Align.CENTER)
        btn_box.set_margin_bottom(10)
        close_btn = Gtk.Button(label="Close")
        close_btn.connect("clicked", self._cancel)
        btn_box.append(close_btn)
        vbox.append(btn_box)

        self._window.show()
        loop = GLib.MainLoop()
        self._window.connect("destroy", lambda *_: loop.quit())
        loop.run()
        return {"result": self.result, "cancelled": self.cancelled}

    def _cancel(self, btn):
        self.cancelled = True
        self._quit()