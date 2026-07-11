import gi
gi.require_version('Gtk', '4.0')
from gi.repository import Gtk, GLib
from filly_graphical.widgets.base import BaseWindow

class UseFlagsWindow(BaseWindow):
    def run(self):
        choices = self.params.get("choices", [])
        self._selected = set()
        self.result = None
        self.cancelled = False

        self._window = Gtk.Window(title=self.title_text or "USE Flags")
        self._window.set_default_size(500, 450)
        self._window.connect("destroy", lambda w: self._quit())

        vbox = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=10)
        vbox.set_margin_top(10); vbox.set_margin_bottom(10)
        vbox.set_margin_start(10); vbox.set_margin_end(10)
        self._window.set_child(vbox)

        title_label = Gtk.Label(label=self.title_text)
        title_label.add_css_class("title-1")
        vbox.append(title_label)

        scrolled = Gtk.ScrolledWindow()
        scrolled.set_hexpand(True); scrolled.set_vexpand(True)
        listbox = Gtk.ListBox()
        listbox.set_selection_mode(Gtk.SelectionMode.NONE)

        for choice in choices:
            row = Gtk.ListBoxRow()
            check = Gtk.CheckButton(label=choice)
            check.connect("toggled", self._on_toggled, choice)
            row.set_child(check)
            listbox.append(row)

        scrolled.set_child(listbox)
        vbox.append(scrolled)

        btn_box = Gtk.Box(spacing=10)
        btn_box.set_halign(Gtk.Align.CENTER)
        ok_btn = Gtk.Button(label="Confirm")
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

    def _on_toggled(self, check, choice):
        if check.get_active():
            self._selected.add(choice)
        else:
            self._selected.discard(choice)

    def _on_ok(self, btn):
        self.result = " ".join(sorted(self._selected))
        self._quit()

    def _on_cancel(self, btn=None):
        self.cancelled = True
        self._quit()