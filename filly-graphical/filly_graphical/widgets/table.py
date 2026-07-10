import gi
gi.require_version('Gtk', '4.0')
from gi.repository import Gtk, GLib
from .base import BaseWindow

class TableWindow(BaseWindow):
    def run(self):
        headers = self.params.get("headers", [])
        rows = self.params.get("rows", [])
        self.result = None
        self.cancelled = False

        self._window = Gtk.Window(title=self.title_text or "Table")
        self._window.set_default_size(700, 400)
        self._window.connect("destroy", lambda w: self._quit())

        vbox = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=10)
        vbox.set_margin_top(10); vbox.set_margin_bottom(10)
        vbox.set_margin_start(10); vbox.set_margin_end(10)
        self._window.set_child(vbox)

        # Header
        title_label = Gtk.Label(label=self.title_text)
        title_label.add_css_class("title-1")
        vbox.append(title_label)

        # Scrolled list
        scrolled = Gtk.ScrolledWindow()
        scrolled.set_hexpand(True); scrolled.set_vexpand(True)
        listbox = Gtk.ListBox()
        listbox.set_selection_mode(Gtk.SelectionMode.SINGLE)
        listbox.connect("row-activated", self._on_row_activated)
        scrolled.set_child(listbox)
        vbox.append(scrolled)

        # Populate rows
        for row_data in rows:
            row = Gtk.ListBoxRow()
            # Build a horizontal box to mimic columns
            hbox = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=20)
            hbox.set_margin_top(5); hbox.set_margin_bottom(5)
            for cell in row_data:
                label = Gtk.Label(label=str(cell), xalign=0)
                hbox.append(label)
            row.set_child(hbox)
            listbox.append(row)

        # Buttons
        btn_box = Gtk.Box(spacing=10)
        btn_box.set_halign(Gtk.Align.CENTER)
        cancel_btn = Gtk.Button(label="Cancel")
        cancel_btn.connect("clicked", self._cancel)
        btn_box.append(cancel_btn)
        vbox.append(btn_box)

        self._window.show()
        loop = GLib.MainLoop()
        self._window.connect("destroy", lambda *_: loop.quit())
        loop.run()
        return {"result": self.result, "cancelled": self.cancelled}

    def _on_row_activated(self, listbox, row):
        # Return the entire row as a list of cell texts
        hbox = row.get_child()
        cells = []
        child = hbox.get_first_child()
        while child is not None:
            if isinstance(child, Gtk.Label):
                cells.append(child.get_label())
            child = child.get_next_sibling()
        self.result = cells
        self._quit()

    def _cancel(self, btn=None):
        self.cancelled = True
        self._quit()