from .base import BaseWindow
from gi.repository import Gtk

class FilterWindow(BaseWindow):
    def run(self):
        self._choices = self.params.get("choices", [])
        self._filtered = list(self._choices)
        self._query = ""

        vbox = self._create_window(500, 400)
        self._entry = Gtk.SearchEntry()
        self._entry.set_placeholder_text(self.params.get("placeholder", "Filter..."))
        self._entry.connect("search-changed", self._on_search_changed)
        vbox.append(self._entry)

        scrolled = Gtk.ScrolledWindow()
        scrolled.set_hexpand(True); scrolled.set_vexpand(True)
        self._listbox = Gtk.ListBox()
        self._listbox.set_selection_mode(Gtk.SelectionMode.SINGLE)
        scrolled.set_child(self._listbox)
        vbox.append(scrolled)

        self._populate()
        vbox.append(self._make_button_box(
            ("OK", lambda b: self._select_current(), True),
            ("Cancel", lambda b: self._cancel(), False),
        ))
        return super().run()

    def _on_search_changed(self, entry):
        self._query = entry.get_text().lower()
        self._filtered = [c for c in self._choices if self._query in c.lower()]
        self._populate()

    def _populate(self):
        while True:
            row = self._listbox.get_row_at_index(0)
            if not row: break
            self._listbox.remove(row)
        for item in self._filtered:
            row = Gtk.ListBoxRow()
            label = Gtk.Label(label=item, xalign=0)
            row.set_child(label)
            self._listbox.append(row)

    def _select_current(self):
        row = self._listbox.get_selected_row()
        if row:
            self.result = row.get_child().get_text()
            self._quit()
        else:
            self.cancelled = True
            self._quit()

    def _cancel(self):
        self.cancelled = True
        self._quit()