import json
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
)

class GuiBackend:
    def run(self, data, title_color=212, accent_color=34):
        widget_type = data.get("widget")
        params = data.get("params", {})
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
        }
        cls = window_cls.get(widget_type)
        if cls is None:
            return {"error": f"Unknown widget: {widget_type}", "cancelled": True}
        window = cls(params, title_color, accent_color)
        return window.run()