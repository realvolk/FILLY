import gi
gi.require_version('Gtk', '4.0')
gi.require_version('Adw', '1')
import json
from gi.repository import Gtk, Adw

class HubPage(Gtk.Box):
    def __init__(self, app, categories, actions, state_prefix=""):
        super().__init__(orientation=Gtk.Orientation.VERTICAL, spacing=0)
        self.app = app
        self.categories = categories
        self.actions = actions
        self.state_prefix = state_prefix
        self._visibility_rows = {}
        self._visibility_conditions = {}

        paned = Gtk.Paned(orientation=Gtk.Orientation.HORIZONTAL)
        paned.set_position(250)

        self.cat_listbox = Gtk.ListBox()
        self.cat_listbox.set_selection_mode(Gtk.SelectionMode.SINGLE)
        for idx, (label, _) in enumerate(categories):
            row = Gtk.ListBoxRow()
            lbl = Gtk.Label(label=label, xalign=0, margin_top=10, margin_bottom=10, margin_start=10)
            row.set_child(lbl)
            self.cat_listbox.append(row)
        self.cat_listbox.connect("row-selected", self._on_cat_selected)
        paned.set_start_child(self.cat_listbox)

        self.right_stack = Gtk.Stack()
        paned.set_end_child(self.right_stack)
        self.append(paned)

        action_box = Gtk.Box(spacing=10)
        action_box.set_halign(Gtk.Align.CENTER)
        action_box.set_margin_top(10)
        action_box.set_margin_bottom(10)
        for label, cb in actions:
            btn = Gtk.Button(label=label)
            if label in ("Proceed", "Build", "Migrate", "Repair", "Install"):
                btn.add_css_class("suggested-action")
            btn.connect("clicked", lambda b, cb=cb: cb())
            action_box.append(btn)
        self.append(action_box)

        for idx, (label, items) in enumerate(categories):
            page = self._build_category_page(items)
            self.right_stack.add_named(page, str(idx))

        if categories:
            self.cat_listbox.select_row(self.cat_listbox.get_row_at_index(0))

    def _on_cat_selected(self, listbox, row):
        if row:
            idx = row.get_index()
            self.right_stack.set_visible_child_name(str(idx))

    def _build_category_page(self, items):
        box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=6)
        box.set_margin_top(10)
        box.set_margin_start(10)
        box.set_margin_end(10)

        for item in items:
            widget_type = item.get("widget", "input")
            label = item.get("label", "")
            key = self.state_prefix + item.get("id", "")
            value = item.get("value", "")
            choices = item.get("choices", [])
            placeholder = item.get("placeholder", "")
            visible_if = item.get("visible_if", {})

            row = Adw.ActionRow(title=label)
            box.append(row)

            if visible_if:
                self._visibility_rows[key] = row
                self._visibility_conditions[key] = visible_if
                self._apply_visibility(key)

            if widget_type in ("menu", "disk_picker"):
                combo = Gtk.DropDown.new(Gtk.StringList.new(choices))
                if value and value in choices:
                    combo.set_selected(choices.index(value))
                elif choices:
                    combo.set_selected(0)
                combo.connect("notify::selected", self._on_combo_changed, key, choices)
                row.add_suffix(combo)
            elif widget_type == "yesno":
                switch = Gtk.Switch()
                switch.set_active(value.lower() == "yes")
                switch.connect("state-set", self._on_switch_changed, key)
                row.add_suffix(switch)
            elif widget_type == "input":
                entry = Gtk.Entry()
                entry.set_text(value)
                if placeholder:
                    entry.set_placeholder_text(placeholder)
                entry.connect("changed", self._on_entry_changed, key)
                row.add_suffix(entry)
            elif widget_type == "password":
                entry = Gtk.PasswordEntry()
                entry.set_show_peek_icon(False)
                if placeholder:
                    entry.set_placeholder_text(placeholder)
                entry.connect("changed", self._on_entry_changed, key)
                row.add_suffix(entry)
            elif widget_type == "filter":
                combo = Gtk.ComboBoxText.new_with_entry()
                combo.set_entry_text_column(0)
                for c in choices:
                    combo.append_text(c)
                if value:
                    combo.get_child().set_text(value)
                combo.connect("changed", self._on_combo_changed_text, key)
                row.add_suffix(combo)
            elif widget_type == "multiselect":
                btn = Gtk.Button(label="Select packages...")
                btn.connect("clicked", self._on_multiselect_clicked, key, label, choices)
                row.add_suffix(btn)
            elif widget_type == "user_manager":
                btn = Gtk.Button(label="Manage Users")
                btn.connect("clicked", self._on_user_manager_clicked, key, label)
                row.add_suffix(btn)
            elif widget_type == "kernel_config":
                btn = Gtk.Button(label="Configure Kernel")
                btn.connect("clicked", self._on_kernel_config_clicked)
                row.add_suffix(btn)
            elif widget_type == "kernel_picker":
                if self.app.state.get("KERNEL_CHOICE", "") == "tkg":
                    tkg_btn = Gtk.Button(label="TKG Configuration")
                    tkg_btn.connect("clicked", self._on_tkg_configure)
                    row.add_suffix(tkg_btn)

        return box

    def _apply_visibility(self, key):
        if key not in self._visibility_rows:
            return
        row = self._visibility_rows[key]
        conditions = self._visibility_conditions.get(key, {})
        visible = True
        for cond_key, cond_val in conditions.items():
            actual = self.app.state.get(cond_key, "")
            if actual != cond_val:
                visible = False
                break
        row.set_visible(visible)

    def _on_state_changed(self, changed_key):
        for key, conditions in self._visibility_conditions.items():
            if changed_key in conditions:
                self._apply_visibility(key)

    def _on_combo_changed(self, combo, param, key, choices):
        idx = combo.get_selected()
        if 0 <= idx < len(choices):
            self.app.state[key] = choices[idx]
            self._on_state_changed(key)

    def _on_combo_changed_text(self, combo, key):
        text = combo.get_child().get_text()
        self.app.state[key] = text

    def _on_switch_changed(self, switch, state, key):
        self.app.state[key] = "yes" if state else "no"
        self._on_state_changed(key)

    def _on_entry_changed(self, entry, key):
        self.app.state[key] = entry.get_text()
        self._on_state_changed(key)

    def _on_multiselect_clicked(self, btn, key, label, choices):
        dialog = Gtk.Dialog(title=f"Select {label}", transient_for=self.app.window, modal=True)
        dialog.set_default_size(400, 400)
        dialog.add_button("Cancel", Gtk.ResponseType.CANCEL)
        dialog.add_button("OK", Gtk.ResponseType.OK)

        vbox = dialog.get_content_area()
        search_entry = Gtk.Entry()
        search_entry.set_placeholder_text("Search...")
        vbox.append(search_entry)

        scroll = Gtk.ScrolledWindow()
        scroll.set_policy(Gtk.PolicyType.NEVER, Gtk.PolicyType.AUTOMATIC)
        checklist = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=3)
        scroll.set_child(checklist)
        vbox.append(scroll)

        selected = self.app.state.get(key, "").split()
        checkboxes = {}

        def populate(query=""):
            while True:
                child = checklist.get_first_child()
                if not child:
                    break
                checklist.remove(child)
            checkboxes.clear()
            for pkg in choices:
                if query.lower() in pkg.lower():
                    cb = Gtk.CheckButton(label=pkg)
                    cb.set_active(pkg in selected)
                    checklist.append(cb)
                    checkboxes[pkg] = cb

        search_entry.connect("search-changed", lambda e: populate(e.get_text()))
        populate()

        dialog.show()
        response = dialog.run()
        if response == Gtk.ResponseType.OK:
            chosen = [pkg for pkg, cb in checkboxes.items() if cb.get_active()]
            self.app.state[key] = " ".join(chosen)
        dialog.destroy()

    def _on_user_manager_clicked(self, btn, key, label):
        dialog = Gtk.Dialog(title="Manage Users", transient_for=self.app.window, modal=True)
        dialog.set_default_size(500, 400)
        dialog.add_button("Cancel", Gtk.ResponseType.CANCEL)
        dialog.add_button("Save", Gtk.ResponseType.OK)

        content = dialog.get_content_area()
        content.set_spacing(10)
        content.set_margin(10)

        listbox = Gtk.ListBox()
        listbox.set_selection_mode(Gtk.SelectionMode.SINGLE)
        listbox.set_hexpand(True)
        listbox.set_vexpand(True)
        scrolled = Gtk.ScrolledWindow()
        scrolled.set_child(listbox)
        content.append(scrolled)

        btn_box = Gtk.Box(spacing=10, halign=Gtk.Align.CENTER)
        add_btn = Gtk.Button(label="Add")
        edit_btn = Gtk.Button(label="Edit")
        delete_btn = Gtk.Button(label="Delete")
        btn_box.append(add_btn)
        btn_box.append(edit_btn)
        btn_box.append(delete_btn)
        content.append(btn_box)

        raw = self.app.state.get(key, "")
        try:
            users = json.loads(raw) if raw else []
        except Exception:
            users = []

        def refresh():
            while True:
                row = listbox.get_row_at_index(0)
                if not row: break
                listbox.remove(row)
            for u in users:
                row = Gtk.ListBoxRow()
                shell = u.get("shell", "/bin/bash")
                groups = ",".join(u.get("groups", []))
                sudo = "sudo" if u.get("sudo") else "nosudo"
                label_widget = Gtk.Label(label=f"{u['name']}  ({shell})  [{sudo}]  Groups: {groups}", xalign=0)
                row.set_child(label_widget)
                listbox.append(row)

        refresh()

        def edit_user(user=None):
            edit_dlg = Gtk.Dialog(title="Add User" if user is None else "Edit User",
                                  transient_for=dialog, modal=True)
            edit_dlg.add_button("Cancel", Gtk.ResponseType.CANCEL)
            edit_dlg.add_button("OK", Gtk.ResponseType.OK)
            edit_content = edit_dlg.get_content_area()
            edit_content.set_spacing(6)
            edit_content.set_margin(10)

            name_entry = Gtk.Entry()
            name_entry.set_placeholder_text("Username")
            name_entry.set_text(user["name"] if user else "")
            edit_content.append(Gtk.Label(label="Username:", xalign=0))
            edit_content.append(name_entry)

            pass_entry = Gtk.PasswordEntry()
            pass_entry.set_show_peek_icon(False)
            pass_entry.set_placeholder_text("Leave empty to keep")
            edit_content.append(Gtk.Label(label="Password:", xalign=0))
            edit_content.append(pass_entry)

            shell_combo = Gtk.DropDown.new_from_strings(["/bin/bash", "/bin/zsh", "/usr/bin/fish"])
            shell_combo.set_selected(0 if not user else (["/bin/bash","/bin/zsh","/usr/bin/fish"].index(user.get("shell","/bin/bash")) if user.get("shell","/bin/bash") in ["/bin/bash","/bin/zsh","/usr/bin/fish"] else 0))
            edit_content.append(Gtk.Label(label="Shell:", xalign=0))
            edit_content.append(shell_combo)

            groups_entry = Gtk.Entry()
            groups_entry.set_text(",".join(user.get("groups", [])) if user else "wheel,audio,video,storage")
            edit_content.append(Gtk.Label(label="Groups:", xalign=0))
            edit_content.append(groups_entry)

            sudo_switch = Gtk.Switch()
            sudo_switch.set_active(user.get("sudo", True) if user else True)
            sudo_box = Gtk.Box(spacing=10)
            sudo_box.append(Gtk.Label(label="Sudo:"))
            sudo_box.append(sudo_switch)
            edit_content.append(sudo_box)

            edit_dlg.show()
            if edit_dlg.run() == Gtk.ResponseType.OK:
                name = name_entry.get_text().strip()
                if not name:
                    edit_dlg.destroy()
                    return
                new_user = {
                    "name": name,
                    "pass": pass_entry.get_text() if pass_entry.get_text() else user.get("pass", "") if user else "",
                    "shell": ["/bin/bash","/bin/zsh","/usr/bin/fish"][shell_combo.get_selected()],
                    "groups": [g.strip() for g in groups_entry.get_text().split(",") if g.strip()],
                    "sudo": sudo_switch.get_active()
                }
                if user:
                    for k in new_user:
                        user[k] = new_user[k]
                else:
                    users.append(new_user)
                refresh()
            edit_dlg.destroy()

        add_btn.connect("clicked", lambda b: edit_user(None))
        edit_btn.connect("clicked", lambda b: (lambda row: edit_user(users[row.get_index()]) if row and row.get_index() < len(users) else None)(listbox.get_selected_row()))
        delete_btn.connect("clicked", lambda b: (lambda row: (users.pop(row.get_index()), refresh()) if row and row.get_index() < len(users) else None)(listbox.get_selected_row()))

        dialog.show()
        if dialog.run() == Gtk.ResponseType.OK:
            self.app.state[key] = json.dumps(users)
        dialog.destroy()

    def _on_kernel_config_clicked(self, btn):
        dialog = Gtk.MessageDialog(
            transient_for=self.app.window, modal=True,
            message_type=Gtk.MessageType.INFO,
            buttons=Gtk.ButtonsType.OK,
            text="Kernel config not implemented in graphical hub yet."
        )
        dialog.show()
        dialog.run()
        dialog.destroy()

    def _on_tkg_configure(self, btn):
        dialog = Gtk.MessageDialog(
            transient_for=self.app.window, modal=True,
            message_type=Gtk.MessageType.INFO,
            buttons=Gtk.ButtonsType.OK,
            text="TKG config not implemented in graphical hub yet."
        )
        dialog.show()
        dialog.run()
        dialog.destroy()