import os
from .base import BaseWindow
from gi.repository import Gtk, GLib

class TextEditorWindow(BaseWindow):
    def run(self):
        self._file_path = self.params.get("file")
        content = self.params.get("content", "")
        if self._file_path and os.path.exists(self._file_path):
            try:
                with open(self._file_path, "r") as f:
                    content = f.read()
            except Exception:
                content = f"[Error reading {self._file_path}]"

        vbox = self._create_window(700, 500)

        scrolled = Gtk.ScrolledWindow()
        scrolled.set_hexpand(True)
        scrolled.set_vexpand(True)
        self._textview = Gtk.TextView()
        self._textview.set_monospace(True)
        self._textview.set_wrap_mode(Gtk.WrapMode.WORD_CHAR)
        self._textview.get_buffer().set_text(content)
        scrolled.set_child(self._textview)
        vbox.append(scrolled)

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

        self.window.show()
        loop = GLib.MainLoop()
        self.window.connect("destroy", lambda *_: loop.quit())
        loop.run()

        return {"result": self.result, "cancelled": self.cancelled}

    def _on_save(self, btn):
        if self._file_path:
            buf = self._textview.get_buffer()
            start = buf.get_start_iter()
            end = buf.get_end_iter()
            text = buf.get_text(start, end, False)
            try:
                with open(self._file_path, "w") as f:
                    f.write(text)
                self.result = True
            except Exception as e:
                self.result = None
                self.error = str(e)
        else:
            buf = self._textview.get_buffer()
            start = buf.get_start_iter()
            end = buf.get_end_iter()
            text = buf.get_text(start, end, False)
            self.result = text
        self._quit()

    def _on_cancel(self, btn):
        self.cancelled = True
        self._quit()