import subprocess, threading, re, os
from gi.repository import Gtk, GLib
from .base import BaseWindow

class ProgressWindow(BaseWindow):
    def __init__(self, params, title_color=212, accent_color=34):
        super().__init__(params, title_color, accent_color)
        self.command = params.get("command", [])
        self.logfile = params.get("logfile")
        self._proc = None
        self._finished = False
        self._progress = 0
        self._stage = "Starting..."
        self._show_raw = False

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
        self.progress_bar.set_text(self._stage)
        vbox.append(self.progress_bar)

        self.spinner = Gtk.Spinner()
        self.spinner.start()
        vbox.append(self.spinner)

        btn_box = Gtk.Box(spacing=10, halign=Gtk.Align.CENTER)
        cancel_btn = Gtk.Button(label="Cancel")
        cancel_btn.connect("clicked", self._cancel)
        btn_box.append(cancel_btn)
        vbox.append(btn_box)

        if self.command:
            threading.Thread(target=self._run_command, daemon=True).start()
            GLib.timeout_add(100, self._pulse_progress)
        else:
            self._finished = True
            self.spinner.stop()
            self.progress_bar.set_fraction(0.0)
            self.progress_bar.set_text("No command")

        return super().run()

    def _run_command(self):
        self._proc = subprocess.Popen(self.command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, bufsize=1)
        for line in self._proc.stdout:
            GLib.idle_add(self._append_log, line)
            self._detect_stage(line)
        self._proc.wait()
        GLib.idle_add(self._finish, self._proc.returncode == 0)

    def _detect_stage(self, line):
        line_lower = line.lower()
        if "preflight" in line_lower:
            self._progress = 5; self._stage = "Preflight complete"
        elif "mount" in line_lower:
            self._progress = 20; self._stage = "Storage configured"
        elif "base system" in line_lower or "bootstrap" in line_lower:
            self._progress = 50; self._stage = "Base installed"
        elif "bootloader" in line_lower or "boot loader" in line_lower:
            self._progress = 78; self._stage = "Bootloader configured"
        elif "post-install" in line_lower or "post install" in line_lower:
            self._progress = 90; self._stage = "Post-install done"
        elif "final" in line_lower or "finalizing" in line_lower:
            self._progress = 100; self._stage = "Finalizing"
        GLib.idle_add(self._update_progress_ui)

    def _update_progress_ui(self):
        self.progress_bar.set_fraction(self._progress / 100.0)
        self.progress_bar.set_text(f"{self._stage} {self._progress}%")

    def _append_log(self, line):
        buf = self.log_view.get_buffer()
        buf.insert(buf.get_end_iter(), line)
        self.log_view.scroll_to_iter(buf.get_end_iter(), 0.0, False, 0.0, 0.0)

    def _pulse_progress(self):
        if not self._finished:
            return True
        self.spinner.stop()
        return False

    def _finish(self, success):
        self._finished = True
        self._progress = 100
        self._stage = "Complete"
        self.progress_bar.set_fraction(1.0)
        self.progress_bar.set_text(self._stage)
        self.result = True

    def _cancel(self, btn):
        if self._proc and self._proc.poll() is None:
            self._proc.terminate()
        self.cancelled = True
        self._quit()

    def _on_key_pressed(self, controller, keyval, keycode, state):
        if keyval == Gdk.KEY_Tab:
            self._show_raw = not self._show_raw
            return True
        return super()._on_key_pressed(controller, keyval, keycode, state)