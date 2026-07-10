import json
from .base import BaseWindow
from gi.repository import Gtk, GLib

class HubWindow(BaseWindow):
    def run(self):
        self._categories = self.params.get("categories", [])
        self._actions = self.params.get("actions", [])
        self._values = {}
        # Initialize values from item defaults
        for cat in self._categories:
            for item in cat.get("items", []):
                key = item.get("id")
                if key:
                    self._values[key] = item.get("value", "")

        self._window = Gtk.Window(title=self.title_text)
        self._window.set_default_size(900, 600)
        self._window.connect("destroy", lambda w: self._quit())

        main_box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=0)
        self._window.set_child(main_box)

        header = Gtk.HeaderBar()
        header.set_show_title_buttons(True)
        self._window.set_titlebar(header)

        paned = Gtk.Paned(orientation=Gtk.Orientation.HORIZONTAL)
        paned.set_position(250)
        main_box.append(paned)

        # Left categories
        self._cat_listbox = Gtk.ListBox()
        self._cat_listbox.set_selection_mode(Gtk.SelectionMode.SINGLE)
        for idx, cat in enumerate(self._categories):
            row = Gtk.ListBoxRow()
            lbl = Gtk.Label(label=cat.get("label", ""), xalign=0, margin_top=10, margin_bottom=10, margin_start=10)
            row.set_child(lbl)
            self._cat_listbox.append(row)
        self._cat_listbox.connect("row-selected", self._on_cat_selected)
        paned.set_start_child(self._cat_listbox)

        # Right items area (stack)
        self._stack = Gtk.Stack()
        for idx, cat in enumerate(self._categories):
            page = self._build_category_page(cat)
            self._stack.add_named(page, str(idx))
        paned.set_end_child(self._stack)

        # Action buttons at bottom
        action_box = Gtk.Box(spacing=10)
        action_box.set_halign(Gtk.Align.CENTER)
        action_box.set_margin_top(10); action_box.set_margin_bottom(10)
        for label in self._actions:
            btn = Gtk.Button(label=label)
            if label in ("Proceed", "Build", "Install"):
                btn.add_css_class("suggested-action")
            btn.connect("clicked", self._make_action_handler(label))
            action_box.append(btn)
        main_box.append(action_box)

        if self._categories:
            self._cat_listbox.select_row(self._cat_listbox.get_row_at_index(0))
        self._window.show()
        loop = GLib.MainLoop()
        self._window.connect("destroy", lambda *_: loop.quit())
        loop.run()
        return {"result": self.result, "cancelled": self.cancelled}

    def _make_action_handler(self, action):
        def handler(btn):
            if action == "Proceed":
                self.result = self._values
                self._quit()
            else:
                # For other actions, return action name as result
                self.result = action
                self._quit()
        return handler

    def _on_cat_selected(self, listbox, row):
        if row:
            idx = row.get_index()
            self._stack.set_visible_child_name(str(idx))

    def _build_category_page(self, cat):
        box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=6)
        box.set_margin_top(10); box.set_margin_start(10); box.set_margin_end(10)
        for item in cat.get("items", []):
            widget_type = item.get("widget", "input")
            label = item.get("label", "")
            key = item.get("id", "")
            choices = item.get("choices", [])
            placeholder = item.get("placeholder", "")
            current_val = self._values.get(key, item.get("value", ""))

            row = Gtk.ListBoxRow()
            hbox = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=10)
            hbox.set_margin_top(5); hbox.set_margin_bottom(5)
            lbl = Gtk.Label(label=label, xalign=0)
            hbox.append(lbl)

            if widget_type in ("menu", "disk_picker"):
                combo = Gtk.DropDown.new_from_strings(choices)
                if current_val in choices:
                    combo.set_selected(choices.index(current_val))
                combo.connect("notify::selected", self._on_combo_changed, key, choices)
                hbox.append(combo)
            elif widget_type == "yesno":
                switch = Gtk.Switch()
                switch.set_active(current_val.lower() == "yes")
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
                entry.connect("search-changed", self._on_entry_changed, key)
                hbox.append(entry)
            elif widget_type == "user_manager":
                btn = Gtk.Button(label="Manage Users")
                btn.connect("clicked", self._on_user_manager)
                hbox.append(btn)
            # etc
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

    def _on_user_manager(self, btn):
        dialog = Gtk.MessageDialog(
            transient_for=self._window, modal=True,
            message_type=Gtk.MessageType.INFO,
            buttons=Gtk.ButtonsType.OK,
            text="User management not implemented in graphical hub yet."
        )
        dialog.show()
        dialog.run()
        dialog.destroy()