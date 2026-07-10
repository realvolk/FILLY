import gi
gi.require_version('Gtk', '4.0')
from gi.repository import Gtk, GLib
import threading
from .base import BaseWindow

class SpinnerWindow(BaseWindow):
    def run(self):
        message = self.params.get("message", "Loading...")
        callback_name = self.params.get("callback", None)
        callback_args = self.params.get("args", [])

        self.result = None
        self.cancelled = False
        self._task_done = False

        self._window = Gtk.Window(title=self.title_text or "Please wait")
        self._window.set_default_size(300, 100)
        self._window.connect("destroy", lambda w: self._quit())

        vbox = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=10)
        vbox.set_margin_top(20); vbox.set_margin_bottom(20)
        vbox.set_margin_start(20); vbox.set_margin_end(20)
        self._window.set_child(vbox)

        spinner = Gtk.Spinner()
        spinner.start()
        spinner.set_halign(Gtk.Align.CENTER)
        vbox.append(spinner)

        label = Gtk.Label(label=message)
        label.set_halign(Gtk.Align.CENTER)
        vbox.append(label)

        # Run callback in background thread if provided
        if callback_name and hasattr(self, callback_name):
            cb = getattr(self, callback_name)
            threading.Thread(target=self._run_callback, args=(cb, callback_args), daemon=True).start()
        else:
            # If no callback, auto-dismiss after 2 seconds
            GLib.timeout_add_seconds(2, self._on_task_finished, True)

        self._window.show()
        loop = GLib.MainLoop()
        self._window.connect("destroy", lambda *_: loop.quit())
        loop.run()
        return {"result": self.result, "cancelled": self.cancelled}

    def _run_callback(self, cb, args):
        try:
            result = cb(*args)
            GLib.idle_add(self._on_task_finished, result)
        except Exception as e:
            GLib.idle_add(self._on_task_failed, str(e))

    def _on_task_finished(self, result):
        if self._task_done:
            return False
        self._task_done = True
        self.result = result
        self._quit()
        return False

    def _on_task_failed(self, error_msg):
        if self._task_done:
            return False
        self._task_done = True
        self.result = None
        self.error = error_msg
        self.cancelled = True
        self._quit()
        return False