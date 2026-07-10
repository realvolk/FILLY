import subprocess, threading, re, os
from gi.repository import Gtk, GLib, Pango
from .base import BaseWindow

class ProgressWindow(BaseWindow):
    def __init__(self, params, title_color=212, accent_color=34):
        super().__init__(params, title_color, accent_color)
        self.command = params.get("command", [])
        self.logfile = params.get("logfile")
        self._proc = None
        self._finished = False

    def run(self):
        vbox = self._create_window(700, 500)
        scrolled = Gtk.ScrolledWindow()
        scrolled.set_hexpand(True)
        scrolled.set_vexpand(True)
        self.log_view = Gtk.TextView()
        self.log_view.set_editable(False)
        self.log_view.set_monospace(True)
        scrolled.set_child(self.log_view)
        vbox.append(scrolled)

        self.progress_bar = Gtk.ProgressBar()
        self.progress_bar.set_show_text(True)
        vbox.append(self.progress_bar)

        self.spinner = Gtk.Spinner()
        self.spinner.start()
        vbox.append(self.spinner)

        cancel_btn = Gtk.Button(label="Cancel")
        cancel_btn.connect("clicked", self._cancel)
        vbox.append(cancel_btn)

        threading.Thread(target=self._run_command, daemon=True).start()
        GLib.timeout_add(100, self._pulse_progress)
        return super().run()

    def _run_command(self):
        self._proc = subprocess.Popen(self.command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, bufsize=1)
        for line in self._proc.stdout:
            GLib.idle_add(self._append_log, line)
        self._proc.wait()
        GLib.idle_add(self._finish, self._proc.returncode == 0)

    def _append_log(self, line):
        buf = self.log_view.get_buffer()
        buf.insert(buf.get_end_iter(), line)
        self.log_view.scroll_to_iter(buf.get_end_iter(), 0.0, False, 0.0, 0.0)

    def _pulse_progress(self):
        if not self._finished:
            self.progress_bar.pulse()
            return True
        return False

    def _finish(self, success):
        self._finished = True
        self.spinner.stop()
        self.progress_bar.set_fraction(1.0)
        self.result = True
        self._quit()

    def _cancel(self, btn):
        if self._proc and self._proc.poll() is None:
            self._proc.terminate()
        self.cancelled = True
        self._quit()