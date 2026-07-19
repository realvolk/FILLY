from gi.repository import Gtk, GLib, Gdk
from .base import BaseWindow

class PasswordConfirmWindow(BaseWindow):
    def run(self):
        self.result = None
        self.cancelled = False

        self._window = Gtk.Window(title=self.title_text or "Set Password", default_width=400, default_height=200)
        self._window.connect("destroy", lambda w: self._quit())
        key_ctrl = Gtk.EventControllerKey.new()
        key_ctrl.connect("key-pressed", self._on_key)
        self._window.add_controller(key_ctrl)

        vbox = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=10, margin_top=20, margin_bottom=20, margin_start=20, margin_end=20)
        self._window.set_child(vbox)

        title_label = Gtk.Label(label=self.title_text)
        title_label.add_css_class("title-1")
        vbox.append(title_label)

        self._pw1 = Gtk.PasswordEntry(placeholder_text="Password", show_peek_icon=False)
        vbox.append(self._pw1)
        self._pw2 = Gtk.PasswordEntry(placeholder_text="Confirm password", show_peek_icon=False)
        vbox.append(self._pw2)

        self._error_label = Gtk.Label(label="")
        self._error_label.add_css_class("error")
        vbox.append(self._error_label)

        btn_box = Gtk.Box(spacing=10, halign=Gtk.Align.CENTER)
        ok_btn = Gtk.Button(label="OK")
        ok_btn.add_css_class("suggested-action")
        ok_btn.connect("clicked", self._on_ok)
        btn_box.append(ok_btn)
        cancel_btn = Gtk.Button(label="Cancel")
        cancel_btn.connect("clicked", self._on_cancel)
        btn_box.append(cancel_btn)
        vbox.append(btn_box)

        self._window.present()
        loop = GLib.MainLoop()
        self._window.connect("destroy", lambda *_: loop.quit())
        loop.run()
        return {"result": self.result, "cancelled": self.cancelled}

    def _on_key(self, controller, keyval, keycode, state):
        if keyval == Gdk.KEY_Escape:
            self.cancelled = True
            self._quit()
            return True
        if keyval in (Gdk.KEY_Return, Gdk.KEY_KP_Enter):
            self._on_ok(None)
            return True
        return False

    def _on_ok(self, btn):
        pw1 = self._pw1.get_text()
        pw2 = self._pw2.get_text()
        if not pw1:
            self._error_label.set_text("Password cannot be empty")
            return
        if pw1 != pw2:
            self._error_label.set_text("Passwords do not match")
            return
        self.result = pw1
        self._quit()

    def _on_cancel(self, btn):
        self.cancelled = True
        self._quit()