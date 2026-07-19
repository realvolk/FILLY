import gi
gi.require_version('Gtk', '4.0')
gi.require_version('Adw', '1')
import os
from gi.repository import Gtk, Adw, GLib, Gdk
from .theme import ACCENT_COLORS
from .mode_select import ModeSelectPage
from .progress import ProgressPage

class InstallerApp(Adw.Application):
    def __init__(self, state_file):
        super().__init__(application_id="com.artixforge.installer")
        self.state_file = state_file
        self.state = {}
        self.load_state()
        self.nav_view = Adw.NavigationView()

        self.window = Adw.ApplicationWindow(application=self)
        self.window.set_default_size(900, 620)
        self.window.set_title("ArtixForge Installer")
        self.window.set_content(self.nav_view)

        header = Adw.HeaderBar()
        header.set_show_end_title_buttons(True)
        self.window.set_titlebar(header)

        self._apply_theme()

        self.nav_view.push(Adw.NavigationPage(
            child=ModeSelectPage(self), title="Select Installation Mode"
        ))

        self.summary_text = Gtk.TextView()
        self.summary_text.set_editable(False)
        self.summary_text.set_monospace(True)
        self.summary_text.set_wrap_mode(Gtk.WrapMode.WORD)

    def load_state(self):
        if os.path.exists(self.state_file):
            try:
                with open(self.state_file, 'r') as f:
                    for line in f:
                        line = line.strip()
                        if '=' in line:
                            key, value = line.split('=', 1)
                            value = value.strip('"').replace('\\"', '"')
                            self.state[key] = value
            except Exception:
                pass

    def save_state(self):
        try:
            os.makedirs(os.path.dirname(self.state_file), exist_ok=True)
            with open(self.state_file, 'w') as f:
                for key, value in self.state.items():
                    f.write(f'{key}="{value}"\n')
        except Exception as e:
            print(f"Error saving state: {e}")

    def _apply_theme(self):
        light = self.state.get("GUI_BACKGROUND", "black") == "white"
        style_manager = Adw.StyleManager.get_default()
        style_manager.set_color_scheme(Adw.ColorScheme.FORCE_LIGHT if light else Adw.ColorScheme.FORCE_DARK)
        title_color_str = self.state.get("GUM_TITLE_COLOR", "212")
        try:
            tc = int(title_color_str)
            accent_tuple = ACCENT_COLORS.get(tc)
            if accent_tuple:
                accent_color = Adw.AccentColor(*accent_tuple)
                style_manager.set_accent_color(accent_color)
        except (ValueError, TypeError):
            pass

    def create_summary_page(self, install_callback):
        page = Adw.PreferencesPage()
        group = Adw.PreferencesGroup(title="Installation Summary")
        scrolled = Gtk.ScrolledWindow()
        scrolled.set_min_content_height(350)
        scrolled.set_child(self.summary_text)
        group.add(scrolled)

        install_btn = Gtk.Button(label="Install")
        install_btn.add_css_class("suggested-action")
        install_btn.set_halign(Gtk.Align.CENTER)
        install_btn.connect("clicked", lambda b: install_callback())
        group.add(install_btn)

        page.add(group)
        return page

    def start_installation(self, mode_title):
        self.save_state()
        base_dir = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
        install_script = os.path.join(base_dir, "..", "install")
        progress_page = ProgressPage(
            title=mode_title,
            command=[install_script],
            state=self.state,
            cwd=os.path.dirname(install_script),
            on_complete=self._on_install_complete
        )
        self.nav_view.push(Adw.NavigationPage(child=progress_page, title=mode_title))

    def _on_install_complete(self, success, cancelled):
        if cancelled:
            msg = "Installation cancelled by user."
        elif success:
            msg = "Installation completed successfully!\nYou may now reboot."
        else:
            msg = "Installation failed.\nCheck /tmp/artix-installer/install.log"
        page = Adw.StatusPage()
        page.set_title("ArtixForge")
        page.set_description(msg)
        self.nav_view.push(Adw.NavigationPage(child=page, title="Result"))


def run_config_mode(state_file):
    app = InstallerApp(state_file)
    app.run(None)