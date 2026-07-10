import gi
gi.require_version('Gtk', '4.0')
from gi.repository import Gtk, GLib
from .base import BaseWindow

class GaugeWindow(BaseWindow):
    def run(self):
        percent = self.params.get("percent", 0)
        label = self.params.get("label", "")

        self._window = Gtk.Window(title=self.title_text or "Gauge")
        self._window.set_default_size(400, 150)
        self._window.connect("destroy", lambda w: self._quit())

        vbox = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=10)
        vbox.set_margin_top(20); vbox.set_margin_bottom(20)
        vbox.set_margin_start(20); vbox.set_margin_end(20)
        self._window.set_child(vbox)

        title_label = Gtk.Label(label=self.title_text)
        title_label.add_css_class("title-1")
        vbox.append(title_label)

        progress = Gtk.ProgressBar()
        progress.set_fraction(percent / 100.0)
        progress.set_show_text(True)
        progress.set_text(f"{label} {percent}%")
        vbox.append(progress)

        # Auto-close after 2 seconds
        GLib.timeout_add_seconds(2, self._quit)
        self.result = None
        self.cancelled = False

        self._window.show()
        loop = GLib.MainLoop()
        self._window.connect("destroy", lambda *_: loop.quit())
        loop.run()
        return {"result": self.result, "cancelled": self.cancelled}