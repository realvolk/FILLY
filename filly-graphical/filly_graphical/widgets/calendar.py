import gi
gi.require_version('Gtk', '4.0')
from gi.repository import Gtk, GLib
from .base import BaseWindow

class CalendarWindow(BaseWindow):
    def run(self):
        self._window = Gtk.Window(title=self.title_text or "Calendar")
        self._window.set_default_size(400, 350)
        self._window.connect("destroy", lambda w: self._quit())

        vbox = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=10)
        vbox.set_margin_top(10); vbox.set_margin_bottom(10)
        vbox.set_margin_start(10); vbox.set_margin_end(10)
        self._window.set_child(vbox)

        title_label = Gtk.Label(label=self.title_text)
        title_label.add_css_class("title-1")
        vbox.append(title_label)

        calendar = Gtk.Calendar()
        calendar.connect("day-selected", self._on_day_selected)
        vbox.append(calendar)

        btn_box = Gtk.Box(spacing=10)
        btn_box.set_halign(Gtk.Align.CENTER)
        select_btn = Gtk.Button(label="Select")
        select_btn.add_css_class("suggested-action")
        select_btn.connect("clicked", lambda b: self._select(calendar))
        btn_box.append(select_btn)
        cancel_btn = Gtk.Button(label="Cancel")
        cancel_btn.connect("clicked", self._cancel)
        btn_box.append(cancel_btn)
        vbox.append(btn_box)

        self.result = None
        self.cancelled = False
        self._window.show()
        loop = GLib.MainLoop()
        self._window.connect("destroy", lambda *_: loop.quit())
        loop.run()
        return {"result": self.result, "cancelled": self.cancelled}

    def _on_day_selected(self, calendar):
        year, month, day = calendar.get_date()
        self._selected = f"{year}-{month+1:02d}-{day:02d}"

    def _select(self, calendar):
        self.result = self._selected
        self._quit()

    def _cancel(self, btn):
        self.cancelled = True
        self._quit()