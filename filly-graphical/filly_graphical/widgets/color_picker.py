import gi
gi.require_version('Gtk', '4.0')
from gi.repository import Gtk, GLib
from .base import BaseWindow

class ColorPickerWindow(BaseWindow):
    def run(self):
        self.result = None
        self.cancelled = False

        dialog = Gtk.ColorChooserDialog(title=self.title_text or "Pick a color")
        dialog.set_modal(True)

        # Pre-select from params if provided
        initial = self.params.get("value", "")
        if initial:
            try:
                rgba = Gdk.RGBA()
                rgba.parse(initial)
                dialog.set_rgba(rgba)
            except Exception:
                pass

        dialog.connect("response", self._on_response)
        dialog.show()

        loop = GLib.MainLoop()
        dialog.connect("destroy", lambda *_: loop.quit())
        loop.run()

        return {"result": self.result, "cancelled": self.cancelled}

    def _on_response(self, dialog, response):
        if response == Gtk.ResponseType.OK:
            rgba = dialog.get_rgba()
            self.result = "#{:02x}{:02x}{:02x}".format(
                int(rgba.red * 255),
                int(rgba.green * 255),
                int(rgba.blue * 255),
            )
        else:
            self.cancelled = True
        dialog.destroy()