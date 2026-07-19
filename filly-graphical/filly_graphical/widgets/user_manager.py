import json
from gi.repository import Gtk, GLib, Gdk
from .base import BaseWindow

class UserManagerWindow(BaseWindow):
    def run(self):
        self._users = self.params.get("users", [])
        self.result = None
        self.cancelled = False

        self._window = Gtk.Window(title=self.title_text or "User Manager", default_width=600, default_height=450)
        self._window.connect("destroy", lambda w: self._quit())
        key_ctrl = Gtk.EventControllerKey.new()
        key_ctrl.connect("key-pressed", self._on_key)
        self._window.add_controller(key_ctrl)

        vbox = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=10, margin_top=10, margin_bottom=10, margin_start=10, margin_end=10)
        self._window.set_child(vbox)

        title_label = Gtk.Label(label=self.title_text)
        title_label.add_css_class("title-1")
        vbox.append(title_label)

        scrolled = Gtk.ScrolledWindow()
        scrolled.set_hexpand(True); scrolled.set_vexpand(True)
        self._listbox = Gtk.ListBox()
        self._listbox.set_selection_mode(Gtk.SelectionMode.SINGLE)
        scrolled.set_child(self._listbox)
        vbox.append(scrolled)

        btn_box = Gtk.Box(spacing=10, halign=Gtk.Align.CENTER)
        add_btn = Gtk.Button(label="Add User")
        add_btn.connect("clicked", self._on_add)
        btn_box.append(add_btn)
        edit_btn = Gtk.Button(label="Edit")
        edit_btn.connect("clicked", self._on_edit)
        btn_box.append(edit_btn)
        delete_btn = Gtk.Button(label="Delete")
        delete_btn.connect("clicked", self._on_delete)
        btn_box.append(delete_btn)
        vbox.append(btn_box)

        action_box = Gtk.Box(spacing=10, halign=Gtk.Align.CENTER)
        save_btn = Gtk.Button(label="Save")
        save_btn.add_css_class("suggested-action")
        save_btn.connect("clicked", self._on_save)
        action_box.append(save_btn)
        cancel_btn = Gtk.Button(label="Cancel")
        cancel_btn.connect("clicked", self._on_cancel)
        action_box.append(cancel_btn)
        vbox.append(action_box)

        self._refresh()
        self._window.present()
        loop = GLib.MainLoop()
        self._window.connect("destroy", lambda *_: loop.quit())
        loop.run()
        return {"result": self.result, "cancelled": self.cancelled}

    def _on_key(self, controller, keyval, keycode, state):
        if keyval == Gdk.KEY_Escape:
            self.cancelled = True
            self._quit()
            return True
        return False

    def _refresh(self):
        while True:
            row = self._listbox.get_row_at_index(0)
            if not row: break
            self._listbox.remove(row)
        for user in self._users:
            row = Gtk.ListBoxRow()
            label = Gtk.Label(label=f"{user.get('username','')}  ({user.get('shell','/bin/bash')})  Groups: {','.join(user.get('groups',[]))}", xalign=0)
            row.set_child(label)
            row._user = user
            self._listbox.append(row)

    def _on_add(self, btn):
        self._edit_user(None)

    def _on_edit(self, btn):
        row = self._listbox.get_selected_row()
        if row and hasattr(row, '_user'):
            self._edit_user(row._user)

    def _on_delete(self, btn):
        row = self._listbox.get_selected_row()
        if row and hasattr(row, '_user'):
            self._users.remove(row._user)
            self._refresh()

    def _edit_user(self, user):
        is_new = user is None
        username = "" if is_new else user.get("username", "")
        shell = "/bin/bash" if is_new else user.get("shell", "/bin/bash")
        groups = [] if is_new else user.get("groups", [])

        dialog = Gtk.Dialog(title="Add User" if is_new else "Edit User", transient_for=self._window, modal=True)
        dialog.add_buttons("OK", Gtk.ResponseType.OK, "Cancel", Gtk.ResponseType.CANCEL)
        content = dialog.get_content_area()
        content.set_spacing(10)
        content.set_margin_top(10); content.set_margin_bottom(10)
        content.set_margin_start(10); content.set_margin_end(10)

        name_entry = Gtk.Entry()
        name_entry.set_placeholder_text("Username")
        name_entry.set_text(username)
        content.append(name_entry)

        shell_entry = Gtk.Entry()
        shell_entry.set_placeholder_text("Shell (e.g., /bin/bash)")
        shell_entry.set_text(shell)
        content.append(shell_entry)

        groups_entry = Gtk.Entry()
        groups_entry.set_placeholder_text("Groups (comma-separated)")
        groups_entry.set_text(",".join(groups))
        content.append(groups_entry)

        dialog.show()
        response = dialog.run()
        if response == Gtk.ResponseType.OK:
            new_name = name_entry.get_text().strip()
            if new_name:
                entry = {"username": new_name,
                         "shell": shell_entry.get_text().strip() or "/bin/bash",
                         "groups": [g.strip() for g in groups_entry.get_text().split(",") if g.strip()]}
                if is_new:
                    self._users.append(entry)
                else:
                    user["username"] = entry["username"]
                    user["shell"] = entry["shell"]
                    user["groups"] = entry["groups"]
                self._refresh()
        dialog.destroy()

    def _on_save(self, btn):
        self.result = self._users
        self._quit()

    def _on_cancel(self, btn):
        self.cancelled = True
        self._quit()