import gi
gi.require_version('Gtk', '4.0')
gi.require_version('Adw', '1')
from gi.repository import Gtk, Adw

class ResumeWizard:
    def __init__(self, app):
        self.app = app

    def push_pages(self):
        summary_page = self.app.create_summary_page(install_callback=self._on_install)
        self._update_summary()
        self.app.nav_view.push(Adw.NavigationPage(child=summary_page, title="Resume Installation"))

    def _update_summary(self):
        if not self.app.state:
            self.app.summary_text.get_buffer().set_text("No saved state found.")
            return
        summary = "Saved Installation Configuration:\n\n"
        for key in ["DISK","FS_TYPE","BOOTLOADER","INIT","KERNEL_CHOICE","WM_DE","DISPLAY_MANAGER","X_STACK","NETWORK_STACK","AUDIO_STACK","USER_SHELL","PRIV_ESCALATION","USE_LUKS","USE_LVM","GENERATE_UKI","HOSTNAME","TIMEZONE","LOCALE","KEYMAP"]:
            value = self.app.state.get(key, "Not set")
            summary += f"  {key}: {value}\n"
        self.app.summary_text.get_buffer().set_text(summary)

    def _on_install(self):
        self.app.state['MODE'] = 'resume'
        self.app.state['GUI_MODE'] = 'yes'
        self.app.save_state()
        self.app.start_installation("Resume Installation")