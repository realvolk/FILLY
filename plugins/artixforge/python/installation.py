import gi
gi.require_version('Gtk', '4.0')
gi.require_version('Adw', '1')
import os, subprocess
from gi.repository import Gtk, Adw
from .hub import HubPage

class InstallationWizard:
    def __init__(self, app):
        self.app = app

    def push_pages(self):
        hub = self.build_install_hub()
        self.app.nav_view.push(Adw.NavigationPage(child=hub, title="Configuration"))

    def build_install_hub(self):
        cats = [
            ("Disk & Storage", self._disk_items()),
            ("Bootloader", self._bootloader_items()),
            ("Kernel & Microcode", self._kernel_items()),
            ("Init System", self._init_items()),
            ("Desktop", self._desktop_items()),
            ("Network & Audio", self._network_audio_items()),
            ("Users & Privilege", self._users_items()),
            ("Extras & Repos", self._extras_items()),
            ("System Identity", self._identity_items()),
            ("Theme", self._theme_items()),
        ]
        actions = [
            ("Quick Profile", self._on_quick_profile),
            ("View Summary", self._on_view_summary),
            ("Proceed", self._on_install_proceed),
        ]
        return HubPage(self.app, cats, actions)

    def _disk_items(self):
        disks = self._get_disks()
        return [
            {"id": "DISK", "label": "Target disk", "value": "", "widget": "disk_picker",
             "disk_picker": True, "choices": disks},
            {"id": "FS_TYPE", "label": "Filesystem", "value": "ext4", "widget": "menu",
             "choices": ["ext4", "btrfs", "xfs", "f2fs"]},
            {"id": "SWAP_ENABLED", "label": "Swap", "value": "no", "widget": "yesno"},
            {"id": "SWAP_SIZE", "label": "Swap size", "value": "0", "widget": "input",
             "placeholder": "e.g. 4G", "visible_if": {"SWAP_ENABLED": "yes"}},
            {"id": "USE_LUKS", "label": "LUKS", "value": "no", "widget": "yesno"},
            {"id": "USE_LVM", "label": "LVM", "value": "no", "widget": "yesno"},
            {"id": "BTRFS_LAYOUT", "label": "BTRFS layout", "value": "standard", "widget": "menu",
             "choices": ["standard", "flat", "snapshot"], "visible_if": {"FS_TYPE": "btrfs"}},
        ]

    def _bootloader_items(self):
        mode = os.environ.get("ARTIX_BOOT_MODE", "uefi")
        bl = ["grub"] if mode == "bios" else ["grub", "refind", "efistub", "limine"]
        return [
            {"id": "BOOTLOADER", "label": "Bootloader", "value": "grub", "widget": "menu", "choices": bl},
            {"id": "GENERATE_UKI", "label": "UKI", "value": "no", "widget": "yesno"},
        ]

    def _kernel_items(self):
        return [
            {"id": "KERNEL_CHOICE", "label": "Kernel", "value": "linux", "widget": "kernel_picker",
             "choices": ["linux", "linux-zen", "linux-lts", "linux-hardened", "linux-libre", "linux-cachyos-bore", "linux-bazzite-bin", "xanmod", "tkg"]},
            {"id": "MICROCODE_OVERRIDE", "label": "Microcode", "value": "auto", "widget": "menu",
             "choices": ["auto", "intel-ucode", "amd-ucode", "none"]},
        ]

    def _init_items(self):
        return [
            {"id": "INIT", "label": "Init system", "value": "openrc", "widget": "menu",
             "choices": ["openrc", "runit", "dinit", "s6"]},
        ]

    def _desktop_items(self):
        return [
            {"id": "WM_DE", "label": "Desktop / WM", "value": "none", "widget": "menu",
             "choices": ["kde", "sonicde", "xfce4", "lxqt", "lxde", "hyprland", "sway", "niri", "i3wm", "dwm", "vxwm", "icewm", "mango", "cinnamon", "budgie", "moksha", "cosmic", "none"]},
            {"id": "DISPLAY_MANAGER", "label": "Display Manager", "value": "none", "widget": "menu",
             "choices": ["none", "lightdm", "sddm", "soniclogin"]},
            {"id": "X_STACK", "label": "Display Stack", "value": "xorg", "widget": "menu",
             "choices": ["xlibre", "xorg"]},
        ]

    def _network_audio_items(self):
        return [
            {"id": "NETWORK_STACK", "label": "Network stack", "value": "networkmanager", "widget": "menu",
             "choices": ["networkmanager", "dhcpcd+iwd", "connman", "none"]},
            {"id": "AUDIO_STACK", "label": "Audio stack", "value": "pipewire", "widget": "menu",
             "choices": ["pipewire", "pulseaudio", "none"]},
        ]

    def _users_items(self):
        return [
            {"id": "USER_COUNT", "label": "User accounts", "value": "0", "widget": "user_manager"},
            {"id": "ROOT_PASS", "label": "Root password", "value": "", "widget": "password", "display": "set/not set"},
            {"id": "PRIV_ESCALATION", "label": "Privilege escalation", "value": "sudo", "widget": "menu",
             "choices": ["sudo", "doas"]},
            {"id": "USER_SHELL", "label": "User shell", "value": "bash", "widget": "menu",
             "choices": ["bash", "zsh", "fish"]},
        ]

    def _extras_items(self):
        return [
            {"id": "EXTRAS", "label": "Extra packages", "value": "", "widget": "multiselect"},
            {"id": "ENABLE_ARCH_REPOS", "label": "Arch repositories", "value": "no", "widget": "yesno"},
            {"id": "ENABLE_AURIS", "label": "AURIS", "value": "no", "widget": "yesno"},
            {"id": "ALLOW_OFFLINE", "label": "Offline mode", "value": "no", "widget": "yesno"},
            {"id": "POWER_USER", "label": "Power User mode", "value": "no", "widget": "yesno"},
        ]

    def _identity_items(self):
        tz = self._get_timezones()
        loc = self._get_locales()
        keymaps = self._get_keymaps()
        return [
            {"id": "HOSTNAME", "label": "Hostname", "value": "artix", "widget": "input"},
            {"id": "TIMEZONE", "label": "Timezone", "value": "Europe/Belgrade", "widget": "filter",
             "placeholder": "e.g. Europe/London", "choices": tz},
            {"id": "LOCALE", "label": "Locale", "value": "en_US.UTF-8", "widget": "filter", "choices": loc},
            {"id": "KEYMAP", "label": "Keyboard layout", "value": "us", "widget": "filter", "choices": keymaps},
        ]

    def _theme_items(self):
        return [
            {"id": "GUM_TITLE_COLOR", "label": "Title color", "value": "212", "widget": "menu",
             "choices": ["212", "39", "245", "250", "3"]},
            {"id": "GUM_ACCENT_COLOR", "label": "Accent color", "value": "34", "widget": "menu",
             "choices": ["34", "117", "196", "255", "11"]},
        ]

    def _get_disks(self):
        try:
            res = subprocess.run(['lsblk', '-dpno', 'NAME,SIZE,MODEL', '-e', '7'], capture_output=True, text=True)
            disks = []
            for line in res.stdout.strip().split('\n'):
                parts = line.split(' ', 2)
                if len(parts) >= 2 and '/dev/sr' not in parts[0] and '/dev/loop' not in parts[0]:
                    disks.append(f"{parts[0]} - {parts[1]} ({parts[2] if len(parts)>2 else ''})")
            return disks
        except:
            return []

    def _get_timezones(self):
        try:
            base = "/usr/share/zoneinfo"
            zones = []
            for root, dirs, files in os.walk(base):
                for f in files:
                    if not f.startswith('.') and not f.endswith('.tab'):
                        path = os.path.join(root, f).replace(base+'/', '')
                        if not path.startswith('posix') and not path.startswith('right'):
                            zones.append(path)
            return sorted(zones)
        except:
            return ["Europe/London", "Europe/Belgrade", "America/New_York"]

    def _get_locales(self):
        try:
            with open('/etc/locale.gen') as f:
                return [l.split()[0] for l in f if l.strip() and not l.startswith('#')]
        except:
            return ["en_US.UTF-8", "en_GB.UTF-8"]

    def _get_keymaps(self):
        try:
            return subprocess.run(['localectl', 'list-keymaps'], capture_output=True, text=True).stdout.strip().split('\n')
        except:
            return ["us", "uk", "de", "fr"]

    def _on_quick_profile(self, btn=None):
        dialog = Gtk.Dialog(title="Quick Profile", transient_for=self.app.window, modal=True)
        dialog.set_default_size(400, 500)
        dialog.add_button("Cancel", Gtk.ResponseType.CANCEL)
        dialog.add_button("Apply", Gtk.ResponseType.OK)
        vbox = dialog.get_content_area()
        vbox.set_spacing(10)
        vbox.set_margin(10)

        profiles = [
            ("KDE Plasma", {"WM_DE":"kde","DISPLAY_MANAGER":"sddm","X_STACK":"xlibre","ENABLE_ARCH_REPOS":"yes","EXTRAS":"git flatpak fastfetch firewalld bluez zram-tools firefox neovim alacritty fzf zoxide starship eza btop htop tmux mpv"}),
            ("XFCE", {"WM_DE":"xfce4","DISPLAY_MANAGER":"lightdm","X_STACK":"xlibre","EXTRAS":"git firefox neovim alacritty fzf zoxide starship eza btop tmux mpv"}),
            ("Hyprland", {"WM_DE":"hyprland","DISPLAY_MANAGER":"none","X_STACK":"none","ENABLE_ARCH_REPOS":"yes","EXTRAS":"git firefox alacritty waybar wofi hyprpaper hyprlock fzf zoxide starship eza btop tmux"}),
            ("Sway", {"WM_DE":"sway","DISPLAY_MANAGER":"none","X_STACK":"none","EXTRAS":"git firefox alacritty waybar wofi swaybg swaylock fzf zoxide starship eza btop tmux"}),
            ("i3wm", {"WM_DE":"i3wm","DISPLAY_MANAGER":"lightdm","X_STACK":"xlibre","ENABLE_ARCH_REPOS":"yes","EXTRAS":"git firefox alacritty fzf zoxide starship eza btop tmux"}),
            ("Server", {"WM_DE":"none","DISPLAY_MANAGER":"none","X_STACK":"none","NETWORK_STACK":"dhcpcd+iwd","AUDIO_STACK":"none","PRIV_ESCALATION":"doas","EXTRAS":"git firewalld zram-tools tmux"}),
            ("Embedded", {"WM_DE":"none","DISPLAY_MANAGER":"none","X_STACK":"none","NETWORK_STACK":"none","AUDIO_STACK":"none","INIT":"busybox","KERNEL_CHOICE":"linux-lts","COREUTILS":"busybox","POWER_USER":"yes","KEEP_BINARY_KERNEL":"no","PRIV_ESCALATION":"none"}),
            ("Volk's Personal", {"WM_DE":"kde","DISPLAY_MANAGER":"lightdm","X_STACK":"xlibre","NETWORK_STACK":"dhcpcd+iwd","INIT":"dinit","PRIV_ESCALATION":"doas","POWER_USER":"yes","KEEP_BINARY_KERNEL":"no","ENABLE_ARCH_REPOS":"yes","EXTRAS":"git fastfetch tmux htop kitty firewalld flatpak"}),
        ]
        listbox = Gtk.ListBox()
        for name, _ in profiles:
            row = Gtk.ListBoxRow()
            row.set_child(Gtk.Label(label=name, xalign=0, margin_top=5, margin_bottom=5))
            listbox.append(row)
        vbox.append(Gtk.Label(label="Select a pre-configured profile:", xalign=0))
        vbox.append(listbox)
        dialog.show()
        if dialog.run() == Gtk.ResponseType.OK:
            row = listbox.get_selected_row()
            if row:
                idx = row.get_index()
                _, settings = profiles[idx]
                for k, v in settings.items():
                    self.app.state[k] = v
                self.app._apply_theme()
        dialog.destroy()

    def _on_view_summary(self, btn=None):
        text = "Installation Summary:\n\n"
        for key, value in sorted(self.app.state.items()):
            if key not in ('LUKS_PASS', 'USER_PASS', 'ROOT_PASS'):
                text += f"{key}: {value}\n"
        self.app.summary_text.get_buffer().set_text(text)
        page = self.app.create_summary_page(install_callback=self._on_install_proceed)
        self.app.nav_view.push(Adw.NavigationPage(child=page, title="Summary"))

    def _on_install_proceed(self, btn=None):
        self.app.state['MODE'] = 'auto'
        self.app.state['GUI_MODE'] = 'yes'
        self.app.save_state()
        self.app.start_installation("Installation")