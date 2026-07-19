import gi, os
gi.require_version('Gtk', '4.0')
gi.require_version('Adw', '1')
from gi.repository import Gtk, Adw
from .hub import HubPage

class ISOWizard:
    def __init__(self, app):
        self.app = app

    def push_pages(self):
        hub = self._build_hub()
        self.app.nav_view.push(Adw.NavigationPage(child=hub, title="ISO Builder"))

    def _build_hub(self):
        cats = [
            ("ISO Type", [
                {"id":"ISO_BOOT_MODE","label":"Boot mode","value":"live","widget":"menu","choices":["live","installer"]},
            ]),
            ("Live Environment", [
                {"id":"WM_DE","label":"Desktop","value":"none","widget":"menu","choices":["kde","sonicde","xfce4","lxqt","lxde","hyprland","sway","niri","i3wm","dwm","vxwm","icewm","mango","cinnamon","budgie","moksha","cosmic","none"]},
                {"id":"DISPLAY_MANAGER","label":"Display Manager","value":"none","widget":"menu","choices":["none","lightdm","sddm","soniclogin"]},
                {"id":"X_STACK","label":"Display Stack","value":"xorg","widget":"menu","choices":["xlibre","xorg"]},
                {"id":"INIT","label":"Init","value":"openrc","widget":"menu","choices":["openrc","runit","dinit","s6"]},
                {"id":"KERNEL_CHOICE","label":"Kernel","value":"linux","widget":"menu","choices":["linux","linux-zen","linux-lts","linux-hardened"]},
                {"id":"NETWORK_STACK","label":"Network","value":"networkmanager","widget":"menu","choices":["networkmanager","dhcpcd+iwd","connman","none"]},
                {"id":"AUDIO_STACK","label":"Audio","value":"pipewire","widget":"menu","choices":["pipewire","pulseaudio","none"]},
            ]),
            ("Output", [
                {"id":"ISO_OUTPUT_DIR","label":"Output directory","value":os.path.expanduser("~/ArtixForge-ISO"),"widget":"input"},
                {"id":"ALLOW_OFFLINE","label":"Offline mode","value":"no","widget":"yesno"},
            ]),
        ]
        actions = [("Build", self._on_build)]
        return HubPage(self.app, cats, actions)

    def _on_build(self):
        self.app.state['MODE'] = 'iso'
        self.app.state['GUI_MODE'] = 'yes'
        self.app.save_state()
        self.app.start_installation("Build ISO")