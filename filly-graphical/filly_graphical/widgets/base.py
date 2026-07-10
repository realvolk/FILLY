import gi
gi.require_version('Gtk', '4.0')
from gi.repository import Gtk, GLib

class BaseWindow:
    def __init__(self, params, title_color=212, accent_color=34):
        self.params = params
        self.title_text = params.get("title", "")
        self.message = params.get("message", "")
        self.result = None
        self.cancelled = False
        self.window = None

    def _create_window(self, default_w=600, default_h=400):
        self.window = Gtk.Window(title=self.title_text or "FILLY")
        self.window.set_default_size(default_w, default_h)
        self.window.connect("destroy", self._on_destroy)
        vbox = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=10)
        vbox.set_margin_top(20)
        vbox.set_margin_bottom(20)
        vbox.set_margin_start(20)
        vbox.set_margin_end(20)
        self.window.set_child(vbox)
        if self.title_text:
            title_label = Gtk.Label(label=self.title_text)
            title_label.add_css_class("title-1")
            title_label.set_halign(Gtk.Align.CENTER)
            vbox.append(title_label)
        if self.message:
            msg_label = Gtk.Label(label=self.message)
            msg_label.set_halign(Gtk.Align.CENTER)
            msg_label.set_wrap(True)
            msg_label.set_max_width_chars(60)
            vbox.append(msg_label)
        return vbox

    def _make_button_box(self, *buttons):
        box = Gtk.Box(spacing=10)
        box.set_halign(Gtk.Align.CENTER)
        for label, cb, primary in buttons:
            btn = Gtk.Button(label=label)
            if primary:
                btn.add_css_class("suggested-action")
            btn.connect("clicked", cb)
            box.append(btn)
        return box

    def _on_destroy(self, widget):
        if not getattr(self, '_finished', False):
            self.cancelled = True

    def _quit(self):
        self._finished = True
        self.window.destroy()

    def run(self):
        self.window.show()
        loop = GLib.MainLoop()
        self.window.connect("destroy", lambda *_: loop.quit())
        loop.run()
        return {"result": self.result, "cancelled": self.cancelled}