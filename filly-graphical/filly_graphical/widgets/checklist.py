from .base import BaseWindow
from gi.repository import Gtk

class ChecklistWindow(BaseWindow):
    def run(self):
        choices = self.params.get("choices", [])
        defaults = set(self.params.get("default", []))
        vbox = self._create_window(500, min(len(choices)*35+150, 600))
        listbox = Gtk.ListBox()
        listbox.set_selection_mode(Gtk.SelectionMode.NONE)
        self._checks = []
        for choice in choices:
            row = Gtk.ListBoxRow()
            check = Gtk.CheckButton(label=choice)
            check.set_active(choice in defaults)
            row.set_child(check)
            listbox.append(row)
            self._checks.append(check)
        vbox.append(listbox)
        vbox.append(self._make_button_box(
            ("OK", lambda b: self._ok(), True),
            ("Cancel", lambda b: self._cancel(), False),
        ))
        return super().run()

    def _ok(self):
        self.result = [c.get_label() for c in self._checks if c.get_active()]
        self._quit()

    def _cancel(self):
        self.cancelled = True
        self._quit()