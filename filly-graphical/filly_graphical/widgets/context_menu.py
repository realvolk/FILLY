import gi
gi.require_version('Gtk', '4.0')
from gi.repository import Gtk, GLib
from .base import BaseWindow

class ContextMenuWindow(BaseWindow):
    def run(self):
        items = self.params.get("items", [])
        self.result = None
        self.cancelled = False

        self._window = Gtk.Window(title="")
        self._window.set_default_size(200, len(items) * 30 + 10)
        self._window.set_decorated(False)
        self._window.set_keep_above(True)
        self._window.connect("destroy", lambda w: self._quit())

        vbox = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=0)
        self._window.set_child(vbox)

        listbox = Gtk.ListBox()
        listbox.set_selection_mode(Gtk.SelectionMode.SINGLE)
        for item in items:
            row = Gtk.ListBoxRow()
            label = Gtk.Label(label=item, xalign=0, margin_top=5, margin_bottom=5)
            row.set_child(label)
            listbox.append(row)
        listbox.connect("row-activated", self._on_selected)
        vbox.append(listbox)

        key_controller = Gtk.EventControllerKey.new()
        key_controller.connect("key-pressed", self._on_key)
        self._window.add_controller(key_controller)

        self._window.show()
        loop = GLib.MainLoop()
        self._window.connect("destroy", lambda *_: loop.quit())
        loop.run()
        return {"result": self.result, "cancelled": self.cancelled}

    def _on_selected(self, listbox, row):
        self.result = row.get_child().get_label()
        self._quit()

    def _on_key(self, controller, keyval, keycode, state):
        if keyval == Gtk.KEY_Escape:
            self.cancelled = True
            self._quit()
        return False