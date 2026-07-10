import os
from .base import BaseWindow
from gi.repository import Gtk

class FilePickerWindow(BaseWindow):
    def run(self):
        self._start_dir = self.params.get("start_dir", "/")
        self._filter_ext = self.params.get("filter", "")
        self._current_dir = self._start_dir
        self._selected_path = None

        self._window = Gtk.Window(title=self.title_text)
        self._window.set_default_size(700, 500)
        self._window.connect("destroy", lambda w: self._quit())

        vbox = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=10)
        vbox.set_margin_top(20); vbox.set_margin_bottom(20)
        vbox.set_margin_start(20); vbox.set_margin_end(20)
        self._window.set_child(vbox)

        self._path_label = Gtk.Label(label=self._current_dir)
        self._path_label.set_halign(Gtk.Align.START)
        vbox.append(self._path_label)

        scrolled = Gtk.ScrolledWindow()
        scrolled.set_hexpand(True); scrolled.set_vexpand(True)
        self._listbox = Gtk.ListBox()
        self._listbox.set_selection_mode(Gtk.SelectionMode.SINGLE)
        self._listbox.connect("row-activated", self._on_entry_activated)
        scrolled.set_child(self._listbox)
        vbox.append(scrolled)

        nav_box = Gtk.Box(spacing=10)
        up_btn = Gtk.Button(label="Up")
        up_btn.connect("clicked", self._go_up)
        nav_box.append(up_btn)
        select_btn = Gtk.Button(label="Select")
        select_btn.add_css_class("suggested-action")
        select_btn.connect("clicked", self._on_select)
        nav_box.append(select_btn)
        cancel_btn = Gtk.Button(label="Cancel")
        cancel_btn.connect("clicked", self._cancel)
        nav_box.append(cancel_btn)
        vbox.append(nav_box)

        self._populate()
        self._window.show()
        loop = GLib.MainLoop()
        self._window.connect("destroy", lambda *_: loop.quit())
        loop.run()
        return {"result": self._selected_path, "cancelled": self.cancelled}

    def _populate(self):
        while True:
            row = self._listbox.get_row_at_index(0)
            if not row: break
            self._listbox.remove(row)
        try:
            entries = os.scandir(self._current_dir)
        except OSError:
            entries = []
        dirs = []
        files = []
        for entry in entries:
            if entry.is_dir():
                dirs.append(entry.name)
            elif self._filter_ext == "" or entry.name.endswith(self._filter_ext):
                files.append(entry.name)
        for d in sorted(dirs):
            row = Gtk.ListBoxRow()
            label = Gtk.Label(label="[DIR] " + d, xalign=0)
            row.set_child(label)
            self._listbox.append(row)
        for f in sorted(files):
            row = Gtk.ListBoxRow()
            label = Gtk.Label(label="      " + f, xalign=0)
            row.set_child(label)
            self._listbox.append(row)

    def _on_entry_activated(self, listbox, row):
        name = row.get_child().get_text()
        if name.startswith("[DIR] "):
            name = name[6:]
            self._current_dir = os.path.join(self._current_dir, name)
            self._path_label.set_text(self._current_dir)
            self._populate()
        else:
            name = name[6:].strip()
            self._selected_path = os.path.join(self._current_dir, name)
            self._quit()

    def _go_up(self, btn):
        parent = os.path.dirname(self._current_dir)
        if parent != self._current_dir:
            self._current_dir = parent
            self._path_label.set_text(self._current_dir)
            self._populate()

    def _on_select(self, btn):
        row = self._listbox.get_selected_row()
        if row:
            self._on_entry_activated(None, row)

    def _cancel(self):
        self.cancelled = True
        self._quit()