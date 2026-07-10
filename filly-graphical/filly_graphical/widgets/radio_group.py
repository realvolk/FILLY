import gi
gi.require_version('Gtk', '4.0')
from gi.repository import Gtk, GLib
from .base import BaseWindow

class RadioGroupWindow(BaseWindow):
    def run(self):
        choices = self.params.get("choices", [])
        default = self.params.get("default", 0)
        self.result = None
        self.cancelled = False

        self._window = Gtk.Window(title=self.title_text or "Radio Group")
        self._window.set_default_size(400, 300)
        self._window.connect("destroy", lambda w: self._quit())

        vbox = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=10)
        vbox.set_margin_top(20); vbox.set_margin_bottom(20)
        vbox.set_margin_start(20); vbox.set_margin_end(20)
        self._window.set_child(vbox)

        title_label = Gtk.Label(label=self.title_text)
        title_label.add_css_class("title-1")
        vbox.append(title_label)

        if self.message:
            msg_label = Gtk.Label(label=self.message)
            msg_label.set_wrap(True)
            vbox.append(msg_label)

        self._radio_buttons = []
        group = None
        for i, choice in enumerate(choices):
            btn = Gtk.CheckButton(label=choice)
            btn.set_group(group)
            if group is None:
                group = btn
            if i == default:
                btn.set_active(True)
            self._radio_buttons.append(btn)
            vbox.append(btn)

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
        for i, rb in enumerate(self._radio_buttons):
            if rb.get_active():
                self.result = self.params.get("choices", [])[i]
                break
        self._quit()

    def _on_cancel(self, btn):
        self.cancelled = True
        self._quit()