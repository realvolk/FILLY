import json
import os
import importlib
import gi
gi.require_version('Gtk', '4.0')
gi.require_version('Adw', '1')
from gi.repository import Gtk, Adw, GLib, Gdk
from .widgets import (
    MenuWindow, YesNoWindow, InputWindow, PasswordWindow,
    ChecklistWindow, MsgWindow, ProgressWindow, FilePickerWindow,
    FilterWindow, MultiselectWindow, SummaryWindow, TextEditorWindow,
    HubWindow, ButtonWindow, ToggleWindow, SpinnerWindow, SeparatorWindow,
    DiskWindow, TableWindow, TreeWindow, GaugeWindow, CalendarWindow,
    FormWindow, TabsWindow, SplitPanesWindow, ContextMenuWindow,
    NotificationWindow, RadioGroupWindow, RangeSliderWindow,
    ColorPickerWindow, BadgeWindow, RichTextWindow, TooltipWindow,
    UserManagerWindow, PasswordConfirmWindow,
)

class GuiBackend:
    window_cls = {
        "menu": MenuWindow,
        "yesno": YesNoWindow,
        "input": InputWindow,
        "password": PasswordWindow,
        "checklist": ChecklistWindow,
        "msg": MsgWindow,
        "progress": ProgressWindow,
        "file_picker": FilePickerWindow,
        "filter": FilterWindow,
        "multiselect": MultiselectWindow,
        "summary": SummaryWindow,
        "text": TextEditorWindow,
        "hub": HubWindow,
        "button": ButtonWindow,
        "toggle": ToggleWindow,
        "spinner": SpinnerWindow,
        "separator": SeparatorWindow,
        "disk": DiskWindow,
        "table": TableWindow,
        "tree": TreeWindow,
        "gauge": GaugeWindow,
        "calendar": CalendarWindow,
        "form": FormWindow,
        "tabs": TabsWindow,
        "split_panes": SplitPanesWindow,
        "context_menu": ContextMenuWindow,
        "notification": NotificationWindow,
        "radio_group": RadioGroupWindow,
        "range_slider": RangeSliderWindow,
        "color_picker": ColorPickerWindow,
        "badge": BadgeWindow,
        "rich_text": RichTextWindow,
        "tooltip": TooltipWindow,
        "user_manager": UserManagerWindow,
        "password_confirm": PasswordConfirmWindow,
    }

    _plugins_loaded = False

    @classmethod
    def register_widget(cls, name, window_class):
        cls.window_cls[name] = window_class

    @classmethod
    def _discover_plugins(cls):
        if cls._plugins_loaded:
            return

        plugin_paths = [
            os.path.expanduser("~/.config/filly/gui-plugins"),
        ]

        filly_root = os.environ.get("FILLY_ROOT", "")
        if filly_root:
            plugin_paths.append(os.path.join(filly_root, "plugins"))

        for base in plugin_paths:
            if not os.path.isdir(base):
                continue
            for plugin_name in os.listdir(base):
                plugin_python = os.path.join(base, plugin_name, "python")
                init_file = os.path.join(plugin_python, "__init__.py")
                if os.path.isfile(init_file):
                    try:
                        spec = importlib.util.spec_from_file_location(
                            f"filly_plugin_{plugin_name}",
                            init_file,
                            submodule_search_locations=[plugin_python],
                        )
                        module = importlib.util.module_from_spec(spec)
                        spec.loader.exec_module(module)
                        if hasattr(module, "register"):
                            module.register()
                    except Exception as e:
                        print(f"Failed to load GUI plugin '{plugin_name}': {e}", file=__import__('sys').stderr)

        cls._plugins_loaded = True

    def run(self, data, title_color=212, accent_color=34):
        self._discover_plugins()

        widget_type = data.get("widget")
        params = data.get("params", {})

        cls = self.window_cls.get(widget_type)
        if cls is None:
            return {"error": f"Unknown widget: {widget_type}", "cancelled": True}

        window = cls(params, title_color, accent_color)
        return window.run()