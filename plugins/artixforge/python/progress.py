import gi
gi.require_version('Gtk', '4.0')
gi.require_version('Adw', '1')
import subprocess, threading, re
from gi.repository import Gtk, Adw, GLib

class ProgressPage(Gtk.Box):
    def __init__(self, title, command, state, cwd, on_complete):
        super().__init__(orientation=Gtk.Orientation.VERTICAL, spacing=10)
        self.command = command
        self.state = state
        self.cwd = cwd
        self.on_complete = on_complete
        self._proc = None
        self._finished = False

        self.set_margin_top(20)
        self.set_margin_bottom(20)
        self.set_margin_start(20)
        self.set_margin_end(20)

        title_label = Gtk.Label(label=title)
        title_label.add_css_class("title-1")
        self.append(title_label)

        scrolled = Gtk.ScrolledWindow()
        scrolled.set_hexpand(True)
        scrolled.set_vexpand(True)
        self.log_view = Gtk.TextView()
        self.log_view.set_editable(False)
        self.log_view.set_monospace(True)
        scrolled.set_child(self.log_view)
        self.append(scrolled)

        self.progress_bar = Gtk.ProgressBar()
        self.progress_bar.set_show_text(True)
        self.progress_bar.set_pulse_step(0.1)
        self.append(self.progress_bar)

        self.spinner = Gtk.Spinner()
        self.spinner.start()
        self.append(self.spinner)

        cancel_btn = Gtk.Button(label="Cancel")
        cancel_btn.connect("clicked", self._cancel)
        self.append(cancel_btn)

        self._pulse_id = GLib.timeout_add(100, self._pulse)
        threading.Thread(target=self._run, daemon=True).start()

    def _pulse(self):
        if not self._finished:
            self.progress_bar.pulse()
            return True
        return False

    def _run(self):
        stages = [
            ("Preflight dependencies installed.", 0.10),
            ("Mount setup completed.", 0.25),
            ("Base system installation complete.", 0.40),
        ]
        if self.state.get("POWER_USER") == "yes":
            stages.append(("All source packages built and installed.", 0.60))
        stages += [
            ("Bootloader setup complete.", 0.70),
            ("Post-install configuration complete.", 0.90),
            ("Applying final system configuration...", 0.95),
        ]
        stage_match = {msg: prog for msg, prog in stages}
        current = 0.0

        self._proc = subprocess.Popen(
            self.command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
            text=True, bufsize=1, cwd=self.cwd
        )
        for line in self._proc.stdout:
            GLib.idle_add(self._append_log, line)
            for msg, prog in stage_match.items():
                if msg in line:
                    current = max(current, prog)
                    GLib.idle_add(self.progress_bar.set_fraction, current)
                    break
        self._proc.wait()
        success = self._proc.returncode == 0
        GLib.idle_add(self._finish, success, False)

    def _append_log(self, line):
        clean = re.sub(r'\x1b\[[0-9;]*[a-zA-Z]', '', line)
        clean = ''.join(c for c in clean if c.isprintable() or c in '\n\r\t ')
        buf = self.log_view.get_buffer()
        buf.insert(buf.get_end_iter(), clean)
        self.log_view.scroll_to_iter(buf.get_end_iter(), 0.0, False, 0.0, 0.0)

    def _finish(self, success, cancelled):
        if self._finished:
            return
        self._finished = True
        self.spinner.stop()
        self.progress_bar.set_fraction(1.0)
        self.on_complete(success, cancelled)

    def _cancel(self, btn):
        if self._proc and self._proc.poll() is None:
            self._proc.terminate()
        self._finish(False, True)