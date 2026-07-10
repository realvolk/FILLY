import gi
gi.require_version('Gtk', '4.0')
from gi.repository import Gtk, GLib
from .base import BaseWindow

class FormWindow(BaseWindow):
    def run(self):
        fields = self.params.get("fields", [])
        submit_label = self.params.get("submit_label", "Submit")
        self._entries = {}
        self.result = None
        self.cancelled = False

        self._window = Gtk.Window(title=self.title_text or "Form")
        self._window.set_default_size(500, 300)
        self._window.connect("destroy", lambda w: self._quit())

        vbox = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=10)
        vbox.set_margin_top(20); vbox.set_margin_bottom(20)
        vbox.set_margin_start(20); vbox.set_margin_end(20)
        self._window.set_child(vbox)

        title_label = Gtk.Label(label=self.title_text)
        title_label.add_css_class("title-1")
        vbox.append(title_label)

        grid = Gtk.Grid()
        grid.set_row_spacing(10)
        grid.set_column_spacing(10)
        vbox.append(grid)

        for i, field in enumerate(fields):
            label = field.get("label", "")
            value = field.get("value", "")
            widget_type = field.get("widget_type", "input")
            placeholder = field.get("placeholder", "")
            choices = field.get("choices", [])

            lbl = Gtk.Label(label=label, xalign=1)
            grid.attach(lbl, 0, i, 1, 1)

            if widget_type in ("input", "password"):
                entry = Gtk.Entry() if widget_type == "input" else Gtk.PasswordEntry()
                entry.set_text(value)
                if placeholder:
                    entry.set_placeholder_text(placeholder)
                grid.attach(entry, 1, i, 1, 1)
                self._entries[label] = entry
            elif widget_type == "menu":
                combo = Gtk.DropDown.new_from_strings(choices)
                if value in choices:
                    combo.set_selected(choices.index(value))
                grid.attach(combo, 1, i, 1, 1)
                self._entries[label] = combo
            elif widget_type == "toggle":
                switch = Gtk.Switch()
                switch.set_active(value.lower() == "true" or value.lower() == "yes")
                grid.attach(switch, 1, i, 1, 1)
                self._entries[label] = switch

        btn_box = Gtk.Box(spacing=10)
        btn_box.set_halign(Gtk.Align.CENTER)
        submit_btn = Gtk.Button(label=submit_label)
        submit_btn.add_css_class("suggested-action")
        submit_btn.connect("clicked", self._on_submit)
        btn_box.append(submit_btn)
        cancel_btn = Gtk.Button(label="Cancel")
        cancel_btn.connect("clicked", self._cancel)
        btn_box.append(cancel_btn)
        vbox.append(btn_box)

        self._window.show()
        loop = GLib.MainLoop()
        self._window.connect("destroy", lambda *_: loop.quit())
        loop.run()
        return {"result": self.result, "cancelled": self.cancelled}

    def _on_submit(self, btn):
        data = {}
        for label, widget in self._entries.items():
            if isinstance(widget, Gtk.Entry):
                data[label] = widget.get_text()
            elif isinstance(widget, Gtk.DropDown):
                model = widget.get_model()
                idx = widget.get_selected()
                if idx >= 0:
                    data[label] = model.get_string(idx)
                else:
                    data[label] = ""
            elif isinstance(widget, Gtk.Switch):
                data[label] = "yes" if widget.get_active() else "no"
        self.result = data
        self._quit()

    def _cancel(self, btn):
        self.cancelled = True
        self._quit()