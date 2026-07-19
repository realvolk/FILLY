import gi
gi.require_version('Gtk', '4.0')
gi.require_version('Adw', '1')
from gi.repository import Gtk, Adw, GLib, Gdk

_app = None

def _get_app():
    global _app
    if _app is None:
        _app = Adw.Application(application_id="dev.filly.graphical")
        _app.connect('activate', lambda app: None)
    return _app

class BaseWindow:
    def __init__(self, params, title_color=212, accent_color=34):
        self.params = params
        self.title_text = params.get("title", "")
        self.message = params.get("message", "")
        self.result = None
        self.cancelled = False
        self.window = None
        self._title_color = title_color
        self._accent_color = accent_color
        self._finished = False

    def _create_window(self, default_w=600, default_h=400):
        app = _get_app()
        self.window = Adw.Window(
            application=app,
            title=self.title_text or "FILLY",
            default_width=default_w,
            default_height=default_h,
        )
        self.window.connect("destroy", self._on_destroy)
        self._add_key_controller()

        style_manager = Adw.StyleManager.get_default()
        style_manager.set_color_scheme(Adw.ColorScheme.PREFER_DARK)

        header = Adw.HeaderBar()
        header.set_show_title(True)
        header.set_title_widget(Gtk.Label(label=self.title_text, css_classes=["title-1"]))

        content = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=12)
        content.set_margin_top(24)
        content.set_margin_bottom(24)
        content.set_margin_start(24)
        content.set_margin_end(24)

        if self.message:
            msg_label = Gtk.Label(label=self.message, wrap=True, max_width_chars=60, halign=Gtk.Align.CENTER)
            content.append(msg_label)

        toolbar = Gtk.Box(orientation=Gtk.Orientation.VERTICAL)
        toolbar.append(header)
        toolbar.append(content)
        self.window.set_content(toolbar)
        return content

    def _add_key_controller(self):
        key_controller = Gtk.EventControllerKey.new()
        key_controller.connect("key-pressed", self._on_key_pressed)
        self.window.add_controller(key_controller)

    def _on_key_pressed(self, controller, keyval, keycode, state):
        if keyval == Gdk.KEY_Escape:
            self.cancelled = True
            self._quit()
            return True
        return False

    def _make_button_box(self, *buttons):
        box = Gtk.Box(spacing=10, halign=Gtk.Align.CENTER)
        for label, cb, primary in buttons:
            btn = Gtk.Button(label=label)
            if primary:
                btn.add_css_class("suggested-action")
            btn.connect("clicked", cb)
            box.append(btn)
        return box

    def _on_destroy(self, widget):
        if not self._finished:
            self.cancelled = True

    def _quit(self):
        if self._finished:
            return
        self._finished = True
        self.window.destroy()

    def run(self):
        app = _get_app()
        self.window.present()
        loop = GLib.MainLoop()
        self.window.connect("destroy", lambda *_: loop.quit())
        if app.get_windows() and app.get_windows()[0] == self.window:
            app.activate()
        loop.run()
        return {"result": self.result, "cancelled": self.cancelled}