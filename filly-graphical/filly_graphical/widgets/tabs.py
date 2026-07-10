import gi
gi.require_version('Gtk', '4.0')
from gi.repository import Gtk, GLib
from .base import BaseWindow

class TabsWindow(BaseWindow):
    def run(self):
        tabs = self.params.get("tabs", [])
        self.result = None
        self.cancelled = False

        self._window = Gtk.Window(title=self.title_text or "Tabs")
        self._window.set_default_size(600, 400)
        self._window.connect("destroy", lambda w: self._quit())

        vbox = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=0)
        self._window.set_child(vbox)

        title_label = Gtk.Label(label=self.title_text)
        title_label.add_css_class("title-1")
        title_label.set_margin_top(10)
        vbox.append(title_label)

        notebook = Gtk.Notebook()
        notebook.set_hexpand(True); notebook.set_vexpand(True)
        vbox.append(notebook)

        for tab_name in tabs:
            page = Gtk.Label(label=f"Content of {tab_name}")
            page.set_margin_top(20)
            notebook.append_page(page, Gtk.Label(label=tab_name))

        btn_box = Gtk.Box(spacing=10)
        btn_box.set_halign(Gtk.Align.CENTER)
        btn_box.set_margin_bottom(10)
        close_btn = Gtk.Button(label="Close")
        close_btn.connect("clicked", self._cancel)
        btn_box.append(close_btn)
        vbox.append(btn_box)

        self._window.show()
        loop = GLib.MainLoop()
        self._window.connect("destroy", lambda *_: loop.quit())
        loop.run()
        return {"result": self.result, "cancelled": self.cancelled}

    def _cancel(self, btn):
        self.cancelled = True
        self._quit()