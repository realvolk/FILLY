import gi
gi.require_version('Gtk', '4.0')
from gi.repository import Gtk, GLib
from filly_graphical.widgets.base import BaseWindow

class Stage3PickerWindow(BaseWindow):
    def run(self):
        choices = self.params.get("choices", [])
        default = self.params.get("default", "")
        self.result = None
        self.cancelled = False

        self._window = Gtk.Window(title=self.title_text or "Stage3")
        self._window.set_default_size(500, 400)
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
        listbox.set_selection_mode(Gtk.SelectionMode.SINGLE)

        for choice in choices:
            row = Gtk.ListBoxRow()
            label = Gtk.Label(label=choice, xalign=0, margin_top=5, margin_bottom=5)
            row.set_child(label)
            listbox.append(row)
            if choice == default:
                listbox.select_row(row)

        listbox.connect("row-activated", self._on_select)
        scrolled.set_child(listbox)
        vbox.append(scrolled)

        cancel_btn = Gtk.Button(label="Cancel")
        cancel_btn.connect("clicked", self._on_cancel)
        vbox.append(cancel_btn)

        self._window.show()
        loop = GLib.MainLoop()
        self._window.connect("destroy", lambda *_: loop.quit())
        loop.run()
        return {"result": self.result, "cancelled": self.cancelled}

    def _on_select(self, listbox, row):
        self.result = row.get_child().get_text()
        self._quit()

    def _on_cancel(self, btn=None):
        self.cancelled = True
        self._quit()