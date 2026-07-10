import gi
gi.require_version('Gtk', '4.0')
from gi.repository import Gtk, GLib
from .base import BaseWindow

class RangeSliderWindow(BaseWindow):
    def run(self):
        min_val = self.params.get("min", 0)
        max_val = self.params.get("max", 100)
        value = self.params.get("value", 50)
        label = self.params.get("label", "")

        self.result = None
        self.cancelled = False

        self._window = Gtk.Window(title=self.title_text or "Range Slider")
        self._window.set_default_size(400, 150)
        self._window.connect("destroy", lambda w: self._quit())

        vbox = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=10)
        vbox.set_margin_top(20); vbox.set_margin_bottom(20)
        vbox.set_margin_start(20); vbox.set_margin_end(20)
        self._window.set_child(vbox)

        title_label = Gtk.Label(label=self.title_text)
        title_label.add_css_class("title-1")
        vbox.append(title_label)

        self._scale = Gtk.Scale.new_with_range(
            Gtk.Orientation.HORIZONTAL, min_val, max_val, 1
        )
        self._scale.set_value(value)
        self._scale.set_hexpand(True)
        vbox.append(self._scale)

        value_label = Gtk.Label(label=f"{label}: {int(value)}")
        self._scale.connect("value-changed", lambda s: value_label.set_text(
            f"{label}: {int(s.get_value())}"
        ))
        vbox.append(value_label)

        btn_box = Gtk.Box(spacing=10)
        btn_box.set_halign(Gtk.Align.CENTER)
        ok_btn = Gtk.Button(label="OK")
        ok_btn.add_css_class("suggested-action")
        ok_btn.connect("clicked", self._on_ok)
        btn_box.append(ok_btn)
        cancel_btn = Gtk.Button(label="Cancel")
        cancel_btn.connect("clicked", self._on_cancel)
        btn_box.append(cancel_btn)
        vbox.append(btn_box)

        self._window.show()
        loop = GLib.MainLoop()
        self._window.connect("destroy", lambda *_: loop.quit())
        loop.run()
        return {"result": self.result, "cancelled": self.cancelled}

    def _on_ok(self, btn):
        self.result = int(self._scale.get_value())
        self._quit()

    def _on_cancel(self, btn):
        self.cancelled = True
        self._quit()