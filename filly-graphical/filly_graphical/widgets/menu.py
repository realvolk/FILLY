from .base import BaseWindow
from gi.repository import Gtk

class MenuWindow(BaseWindow):
    def run(self):
        choices = self.params.get("choices", [])
        default = self.params.get("default", "")
        vbox = self._create_window(500, min(len(choices)*35+150, 600))
        listbox = Gtk.ListBox()
        listbox.set_selection_mode(Gtk.SelectionMode.SINGLE)
        listbox.set_vexpand(True)
        for choice in choices:
            row = Gtk.ListBoxRow()
            label = Gtk.Label(label=choice, xalign=0, margin_top=5, margin_bottom=5)
            row.set_child(label)
            listbox.append(row)
            if choice == default:
                listbox.select_row(row)
        listbox.connect("row-activated", self._on_select)
        vbox.append(listbox)
        vbox.append(self._make_button_box(("Cancel", lambda b: self._cancel(), False)))
        return super().run()

    def _on_select(self, listbox, row):
        self.result = row.get_child().get_text()
        self._quit()

    def _cancel(self):
        self.cancelled = True
        self._quit()