import json
from .base import BaseWindow
from gi.repository import Gtk, Adw, GLib, Gdk

class HubWindow(BaseWindow):
    def run(self):
        self._categories = self.params.get("categories", [])
        self._actions = self.params.get("actions", [])
        self._values = {}
        for cat in self._categories:
            for item in cat.get("items", []):
                key = item.get("id")
                if key:
                    self._values[key] = item.get("value", "")

        app = Adw.Application(application_id="dev.filly.hub")
        app.connect('activate', lambda app: None)
        self._window = Adw.Window(
            application=app,
            title=self.title_text,
            default_width=1000,
            default_height=700,
        )
        self._window.connect("destroy", lambda w: self._quit())
        key_ctrl = Gtk.EventControllerKey.new()
        key_ctrl.connect("key-pressed", self._on_hub_key)
        self._window.add_controller(key_ctrl)

        style_manager = Adw.StyleManager.get_default()
        style_manager.set_color_scheme(Adw.ColorScheme.PREFER_DARK)

        main_box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=0)

        header = Adw.HeaderBar()
        header.set_show_title(True)
        header.set_title_widget(Gtk.Label(label=self.title_text, css_classes=["title-1"]))
        main_box.append(header)

        paned = Gtk.Paned(orientation=Gtk.Orientation.HORIZONTAL)
        paned.set_position(260)
        main_box.append(paned)

        self._cat_listbox = Gtk.ListBox(css_classes=["navigation-sidebar"])
        self._cat_listbox.set_selection_mode(Gtk.SelectionMode.SINGLE)
        for cat in self._categories:
            row = Gtk.ListBoxRow()
            lbl = Gtk.Label(label=cat.get("label", ""), xalign=0, margin_top=12, margin_bottom=12, margin_start=16)
            row.set_child(lbl)
            self._cat_listbox.append(row)
        self._cat_listbox.connect("row-selected", self._on_cat_selected)
        paned.set_start_child(self._cat_listbox)

        self._stack = Gtk.Stack()
        for idx, cat in enumerate(self._categories):
            page = self._build_category_page(cat)
            self._stack.add_named(page, str(idx))
        paned.set_end_child(self._stack)

        action_box = Gtk.Box(spacing=10, halign=Gtk.Align.CENTER, margin_top=12, margin_bottom=12)
        for label in self._actions:
            btn = Gtk.Button(label=label)
            if label in ("Proceed", "Build", "Install"):
                btn.add_css_class("suggested-action")
            btn.connect("clicked", self._make_action_handler(label))
            action_box.append(btn)
        main_box.append(action_box)

        self._window.set_content(main_box)

        if self._categories:
            self._cat_listbox.select_row(self._cat_listbox.get_row_at_index(0))

        self._window.present()
        loop = GLib.MainLoop()
        self._window.connect("destroy", lambda *_: loop.quit())
        loop.run()
        return {"result": self.result, "cancelled": self.cancelled}

    def _on_hub_key(self, controller, keyval, keycode, state):
        if keyval == Gdk.KEY_Escape:
            self.cancelled = True
            self._quit()
            return True
        return False

    def _make_action_handler(self, action):
        def handler(btn):
            if action == "Proceed":
                self.result = self._values
                self._quit()
            else:
                self.result = action
                self._quit()
        return handler

    def _on_cat_selected(self, listbox, row):
        if row:
            idx = row.get_index()
            self._stack.set_visible_child_name(str(idx))

    def _build_category_page(self, cat):
        box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=6, margin_top=10, margin_start=16, margin_end=16)
        for item in cat.get("items", []):
            widget_type = item.get("widget", "input")
            label = item.get("label", "")
            key = item.get("id", "")
            choices = item.get("choices", [])
            placeholder = item.get("placeholder", "")
            current_val = self._values.get(key, item.get("value", ""))

            row = Gtk.ListBoxRow()
            hbox = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=12, margin_top=6, margin_bottom=6)
            lbl = Gtk.Label(label=label, xalign=0, width_chars=20)
            hbox.append(lbl)

            if widget_type in ("menu", "disk_picker"):
                combo = Gtk.DropDown.new_from_strings(choices)
                if current_val in choices:
                    combo.set_selected(choices.index(current_val))
                combo.connect("notify::selected", self._on_combo_changed, key, choices)
                hbox.append(combo)
            elif widget_type == "yesno":
                switch = Gtk.Switch()
                switch.set_active(str(current_val).lower() in ("yes", "true", "1"))
                switch.connect("state-set", self._on_switch_changed, key)
                hbox.append(switch)
            elif widget_type in ("input", "password"):
                entry = Gtk.Entry() if widget_type == "input" else Gtk.PasswordEntry()
                entry.set_text(current_val)
                if placeholder:
                    entry.set_placeholder_text(placeholder)
                entry.connect("changed", self._on_entry_changed, key)
                hbox.append(entry)
            elif widget_type == "filter":
                entry = Gtk.SearchEntry()
                entry.set_text(current_val)
                entry.set_placeholder_text(placeholder or "Filter...")
                entry.connect("search-changed", self._on_entry_changed, key)
                hbox.append(entry)
            elif widget_type == "multiselect":
                btn = Gtk.Button(label="Choose...")
                btn.connect("clicked", self._on_multiselect, key, choices, current_val)
                hbox.append(btn)
            elif widget_type == "checklist":
                btn = Gtk.Button(label="Choose...")
                btn.connect("clicked", self._on_checklist, key, choices, current_val)
                hbox.append(btn)
            elif widget_type == "disk":
                btn = Gtk.Button(label="Edit partitions")
                btn.connect("clicked", self._on_disk_edit, key, current_val)
                hbox.append(btn)
            elif widget_type == "user_manager":
                btn = Gtk.Button(label="Manage Users")
                btn.connect("clicked", self._on_user_manager, key)
                hbox.append(btn)
            elif widget_type == "password_confirm":
                btn = Gtk.Button(label="Set password")
                btn.connect("clicked", self._on_password_confirm, key)
                hbox.append(btn)
            elif widget_type == "calendar":
                btn = Gtk.Button(label=current_val or "Pick date")
                btn.connect("clicked", self._on_calendar, key, current_val)
                hbox.append(btn)
            elif widget_type == "file_picker":
                btn = Gtk.Button(label=current_val or "Browse...")
                btn.connect("clicked", self._on_file_picker, key, current_val)
                hbox.append(btn)
            else:
                entry = Gtk.Entry()
                entry.set_text(current_val)
                entry.connect("changed", self._on_entry_changed, key)
                hbox.append(entry)

            row.set_child(hbox)
            box.append(row)
        return box

    def _on_combo_changed(self, combo, param, key, choices):
        idx = combo.get_selected()
        if 0 <= idx < len(choices):
            self._values[key] = choices[idx]

    def _on_switch_changed(self, switch, state, key):
        self._values[key] = "yes" if state else "no"

    def _on_entry_changed(self, entry, key):
        self._values[key] = entry.get_text()

    def _on_multiselect(self, btn, key, choices, current_val):
        from .multiselect import MultiselectWindow
        selected = [s.strip() for s in current_val.split(",") if s.strip()]
        params = {"title": key, "choices": choices, "default": selected}
        result = self._run_sub_window(MultiselectWindow, params)
        if result.get("result") and not result.get("cancelled"):
            self._values[key] = ",".join(result["result"])

    def _on_checklist(self, btn, key, choices, current_val):
        from .checklist import ChecklistWindow
        defaults = current_val.split(",") if current_val else []
        params = {"title": key, "choices": choices, "default": defaults}
        result = self._run_sub_window(ChecklistWindow, params)
        if result.get("result") and not result.get("cancelled"):
            self._values[key] = ",".join(result["result"])

    def _on_disk_edit(self, btn, key, current_val):
        from .disk import DiskWindow
        params = {"title": key, "disk": current_val, "partitions": [], "free_space": [], "readonly": False}
        result = self._run_sub_window(DiskWindow, params)
        if result.get("result") and not result.get("cancelled") and isinstance(result["result"], dict):
            self._values[key] = json.dumps(result["result"])

    def _on_user_manager(self, btn, key):
        from .user_manager import UserManagerWindow
        current = self._values.get(key, "")
        try:
            users = json.loads(current) if current else []
        except Exception:
            users = []
        params = {"title": key, "users": users}
        result = self._run_sub_window(UserManagerWindow, params)
        if result.get("result") and not result.get("cancelled"):
            self._values[key] = json.dumps(result["result"])

    def _on_password_confirm(self, btn, key):
        from .password_confirm import PasswordConfirmWindow
        params = {"title": key}
        result = self._run_sub_window(PasswordConfirmWindow, params)
        if result.get("result") and not result.get("cancelled"):
            self._values[key] = result["result"]

    def _on_calendar(self, btn, key, current_val):
        from .calendar import CalendarWindow
        params = {"title": key, "value": current_val}
        result = self._run_sub_window(CalendarWindow, params)
        if result.get("result") and not result.get("cancelled"):
            self._values[key] = result["result"]

    def _on_file_picker(self, btn, key, current_val):
        from .file_picker import FilePickerWindow
        params = {"title": key, "start_dir": current_val or "/", "filter": ""}
        result = self._run_sub_window(FilePickerWindow, params)
        if result.get("result") and not result.get("cancelled"):
            self._values[key] = result["result"]

    def _run_sub_window(self, window_cls, params):
        dlg = window_cls(params)
        dlg._window = Gtk.Window(title=params.get("title", ""), transient_for=self._window, modal=True, default_width=600, default_height=400)
        dlg._create_window = lambda w=600, h=400: self._modal_content(dlg, w, h)
        dlg.run = lambda: self._run_modal(dlg)
        result = dlg.run()
        return result

    def _modal_content(self, dlg, w, h):
        dlg.window.set_default_size(w, h)
        vbox = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=10, margin_top=20, margin_bottom=20, margin_start=20, margin_end=20)
        dlg.window.set_child(vbox)
        return vbox

    def _run_modal(self, dlg):
        dlg.window.present()
        loop = GLib.MainLoop()
        dlg.window.connect("destroy", lambda *_: loop.quit())
        loop.run()
        return {"result": dlg.result, "cancelled": dlg.cancelled}