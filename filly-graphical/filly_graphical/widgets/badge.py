from .base import BaseWindow
from gi.repository import Gtk

class BadgeWindow(BaseWindow):
    def run(self):
        text = self.params.get("text", "")
        color = self.params.get("color", "green")

        self._window = Gtk.Window(title="")
        self._window.set_default_size(100, 30)
        self._window.set_decorated(False)

        label = Gtk.Label(label=text)
        label.add_css_class("caption")
        label.set_margin_top(5); label.set_margin_bottom(5)
        label.set_margin_start(10); label.set_margin_end(10)
        label.set_accessible_label(f"Badge: {text}")
        label.set_accessible_role("status")

        # Apply colored background via CSS
        css_provider = Gtk.CssProvider()
        css_provider.load_from_string(f"""
            label {{
                background-color: {color};
                color: white;
                border-radius: 10px;
                font-weight: bold;
            }}
        """)
        label.get_style_context().add_provider(css_provider, Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION)

        self._window.set_child(label)
        self._window.show()
        self.result = None
        self.cancelled = False
        self._quit()
        return {"result": self.result, "cancelled": self.cancelled}