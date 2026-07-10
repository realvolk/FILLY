from .base import BaseWindow
from gi.repository import Gtk

class MultiselectWindow(BaseWindow):
    def run(self):
        self._choices = self.params.get("choices", [])
        self._selected = set()
        self._query = ""

        vbox = self._create_window(500, 400)
        self._entry = Gtk.SearchEntry()
        self._entry.set_placeholder_text(self.params.get("placeholder", "Filter..."))
        self._entry.connect("search-changed", self._on_search_changed)
        vbox.append(self._entry)

        scrolled = Gtk.ScrolledWindow()
        scrolled.set_hexpand(True); scrolled.set_vexpand(True)
        self._listbox = Gtk.ListBox()
        self._listbox.set_selection_mode(Gtk.SelectionMode.NONE)
        scrolled.set_child(self._listbox)
        vbox.append(scrolled)

        self._populate()
        vbox.append(self._make_button_box(
            ("OK", lambda b: self._ok(), True),
            ("Cancel", lambda b: self._cancel(), False),
        ))
        return super().run()

    def _on_search_changed(self, entry):
        self._query = entry.get_text().lower()
        self._populate()

    def _populate(self):
        while True:
            row = self._listbox.get_row_at_index(0)
            if not row: break
            self._listbox.remove(row)
        for choice in self._choices:
            if self._query and self._query not in choice.lower():
                continue
            row = Gtk.ListBoxRow()
            check = Gtk.CheckButton(label=choice)
            check.set_active(choice in self._selected)
            check.connect("toggled", self._on_toggled, choice)
            row.set_child(check)
            self._listbox.append(row)

    def _on_toggled(self, check, choice):
        if check.get_active():
            self._selected.add(choice)
        else:
            self._selected.discard(choice)

    def _ok(self):
        self.result = list(self._selected)
        self._quit()

    def _cancel(self):
        self.cancelled = True
        self._quit()