import gi
gi.require_version('Gtk', '4.0')
from gi.repository import Gtk, GLib, Gdk
import cairo
from .base import BaseWindow

class DiskWindow(BaseWindow):
    def run(self):
        self._disk = self.params.get("disk", "")
        self._partitions = self.params.get("partitions", [])
        self._free_space = self.params.get("free_space", [])
        self._readonly = self.params.get("readonly", False)
        self._selected = 0
        self.result = None
        self.cancelled = False

        self._window = Gtk.Window(title=f"{self.title_text} ({self._disk})", default_width=900, default_height=600)
        self._window.connect("destroy", lambda w: self._quit())
        self._add_key_controller_to_window()

        vbox = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=10)
        vbox.set_margin_top(10); vbox.set_margin_bottom(10)
        vbox.set_margin_start(10); vbox.set_margin_end(10)
        self._window.set_child(vbox)

        self._bar_drawing = Gtk.DrawingArea()
        self._bar_drawing.set_content_height(40)
        self._bar_drawing.set_draw_func(self._draw_partition_bar)
        self._bar_drawing.set_accessible_label("Disk partition layout")
        vbox.append(self._bar_drawing)

        scrolled = Gtk.ScrolledWindow()
        scrolled.set_hexpand(True); scrolled.set_vexpand(True)
        self._listbox = Gtk.ListBox()
        self._listbox.set_selection_mode(Gtk.SelectionMode.SINGLE)
        self._listbox.connect("row-selected", self._on_row_selected)
        scrolled.set_child(self._listbox)
        vbox.append(scrolled)

        self._detail_label = Gtk.Label()
        self._detail_label.set_xalign(0)
        vbox.append(self._detail_label)

        if not self._readonly:
            btn_box = Gtk.Box(spacing=5)
            btn_box.set_halign(Gtk.Align.CENTER)
            for label, handler in [
                ("New", self._on_new), ("Delete", self._on_delete),
                ("Resize", self._on_resize), ("Type", self._on_type),
                ("Flags", self._on_flags), ("Write", self._on_write),
            ]:
                btn = Gtk.Button(label=label)
                btn.connect("clicked", handler)
                btn_box.append(btn)
            vbox.append(btn_box)

        close_btn = Gtk.Button(label="Close")
        close_btn.connect("clicked", lambda b: self._quit())
        vbox.append(close_btn)

        self._refresh()
        self._window.present()
        loop = GLib.MainLoop()
        self._window.connect("destroy", lambda *_: loop.quit())
        loop.run()
        return {"result": self.result, "cancelled": self.cancelled}

    def _add_key_controller_to_window(self):
        key_ctrl = Gtk.EventControllerKey.new()
        key_ctrl.connect("key-pressed", self._on_key)
        self._window.add_controller(key_ctrl)

    def _on_key(self, controller, keyval, keycode, state):
        if keyval == Gdk.KEY_Escape:
            self.cancelled = True
            self._quit()
            return True
        if keyval == Gdk.KEY_Up:
            if self._selected > 0:
                self._selected -= 1
                self._refresh()
            return True
        if keyval == Gdk.KEY_Down:
            total = len(self._partitions) + len(self._free_space)
            if self._selected + 1 < total:
                self._selected += 1
                self._refresh()
            return True
        return False

    def _refresh(self):
        while True:
            row = self._listbox.get_row_at_index(0)
            if not row: break
            self._listbox.remove(row)

        for i, p in enumerate(self._partitions):
            row = Gtk.ListBoxRow()
            label = Gtk.Label(label=f"  {p.get('number', '')}  {p.get('size', '')}  {p.get('type', '')}", xalign=0)
            if i == self._selected:
                label.set_markup(f"<b>  {p.get('number', '')}  {p.get('size', '')}  {p.get('type', '')}</b>")
            row.set_child(label)
            self._listbox.append(row)

        for i, fs in enumerate(self._free_space):
            idx = len(self._partitions) + i
            row = Gtk.ListBoxRow()
            label = Gtk.Label(label=f"       {fs.get('size', '')}  Free space", xalign=0)
            if idx == self._selected:
                label.set_markup(f"<b>       {fs.get('size', '')}  Free space</b>")
            row.set_child(label)
            self._listbox.append(row)

        self._bar_drawing.queue_draw()
        self._update_detail()

    def _update_detail(self):
        total = len(self._partitions)
        if self._selected < total and self._partitions:
            p = self._partitions[self._selected]
            self._detail_label.set_text(
                f"Partition {p.get('number','')}  Type: {p.get('type','')}  "
                f"Size: {p.get('size','')}  FS: {p.get('fs_signature','none')}  "
                f"Flags: {', '.join(p.get('flags',[])) or 'none'}"
            )
        elif self._free_space:
            idx = self._selected - total
            if 0 <= idx < len(self._free_space):
                fs = self._free_space[idx]
                self._detail_label.set_text(f"Free space  Size: {fs.get('size','')}")
        else:
            self._detail_label.set_text("")

    def _draw_partition_bar(self, area, cr, width, height):
        all_entries = []
        for p in self._partitions:
            all_entries.append((p.get("type", ""), p.get("start", "0"), p.get("end", "0")))
        for fs in self._free_space:
            all_entries.append(("Free", fs.get("start", "0"), fs.get("end", "0")))

        if not all_entries:
            return

        def to_bytes(s):
            s = s.upper().strip()
            if s.endswith("TIB") or s.endswith("TB"):
                return float(s[:-3].strip()) * 1024**4
            elif s.endswith("GIB") or s.endswith("GB"):
                return float(s[:-3].strip()) * 1024**3
            elif s.endswith("MIB") or s.endswith("MB"):
                return float(s[:-3].strip()) * 1024**2
            elif s.endswith("KIB") or s.endswith("KB"):
                return float(s[:-3].strip()) * 1024
            else:
                return float(s.rstrip("B").strip() or 0)

        entries = [(t, to_bytes(s), to_bytes(e)) for t, s, e in all_entries]
        total = max(e for _, _, e in entries)
        if total == 0:
            return

        colors = [
            (0.2, 0.4, 0.8), (0.2, 0.7, 0.7), (0.7, 0.2, 0.7),
            (0.2, 0.7, 0.2), (0.8, 0.2, 0.2), (0.8, 0.7, 0.2),
        ]
        x = 0
        for i, (label, start, end) in enumerate(entries):
            w = (end - start) / total * width
            color = colors[i % len(colors)]
            cr.set_source_rgb(*color)
            cr.rectangle(x, 0, w, height)
            cr.fill()
            cr.set_source_rgb(1, 1, 1)
            cr.set_font_size(12)
            cr.move_to(x + 4, height / 2 + 4)
            cr.show_text(label[:12])
            x += w

    def _on_row_selected(self, listbox, row):
        if row is None: return
        self._selected = row.get_index()
        self._update_detail()

    def _get_free_space_index(self):
        return self._selected - len(self._partitions)

    def _on_new(self, btn):
        fs_idx = self._get_free_space_index()
        if fs_idx < 0 or fs_idx >= len(self._free_space):
            return
        fs = self._free_space[fs_idx]
        self.result = {"_action": "new", "free_space_index": fs_idx, "free_space": dict(fs)}
        self._quit()

    def _on_delete(self, btn):
        if self._selected >= len(self._partitions) or not self._partitions:
            return
        p = self._partitions[self._selected]
        self._partitions.pop(self._selected)
        start = p.get("start", "0")
        end = p.get("end", "0")
        size = self._human_size_bytes(p.get("size", "0"))
        if self._free_space:
            last = self._free_space[-1]
            if last.get("end", "0") == start:
                last["end"] = end
                last["size"] = self._bytes_human(self._human_size_bytes(last.get("size", "0")) + size)
            else:
                self._free_space.append({"start": start, "end": end, "size": p.get("size", "0")})
        else:
            self._free_space.append({"start": start, "end": end, "size": p.get("size", "0")})
        if self._selected >= len(self._partitions) and self._selected > 0:
            self._selected -= 1
        self._refresh()

    def _on_resize(self, btn):
        if self._selected >= len(self._partitions) or not self._partitions:
            return
        p = self._partitions[self._selected]
        self.result = {"_action": "resize", "partition_index": self._selected, "partition": dict(p)}
        self._quit()

    def _on_type(self, btn):
        if self._selected >= len(self._partitions) or not self._partitions:
            return
        p = self._partitions[self._selected]
        types = ["EFI System", "BIOS boot", "Linux filesystem", "Linux swap",
                 "Linux LVM", "Linux LUKS", "Linux RAID", "Linux /boot",
                 "Linux /home", "Linux /var", "Linux /tmp", "Windows data",
                 "Windows recovery", "FreeBSD", "FreeBSD swap", "FreeBSD ZFS",
                 "FreeBSD UFS", "macOS APFS", "macOS HFS+", "Solaris", "Custom"]
        dialog = Gtk.AlertDialog()
        dialog.set_message("Select partition type")
        dialog.set_buttons(types)
        dialog.set_cancel_button("Cancel")
        dialog.choose(self._window, None, self._on_type_chosen, p)

    def _on_type_chosen(self, dialog, result, p):
        try:
            choice = dialog.choose_finish(result)
        except Exception:
            return
        if choice >= 0 and choice < 21:
            types = ["EFI System", "BIOS boot", "Linux filesystem", "Linux swap",
                     "Linux LVM", "Linux LUKS", "Linux RAID", "Linux /boot",
                     "Linux /home", "Linux /var", "Linux /tmp", "Windows data",
                     "Windows recovery", "FreeBSD", "FreeBSD swap", "FreeBSD ZFS",
                     "FreeBSD UFS", "macOS APFS", "macOS HFS+", "Solaris", "Custom"]
            p["type"] = types[choice]
            self._refresh()

    def _on_flags(self, btn):
        if self._selected >= len(self._partitions) or not self._partitions:
            return
        p = self._partitions[self._selected]
        flags = ["boot", "esp", "bios_grub", "lvm", "raid"]
        current_flags = p.get("flags", [])
        dialog = Gtk.Dialog(title="Partition Flags", transient_for=self._window, modal=True)
        dialog.add_buttons("OK", Gtk.ResponseType.OK, "Cancel", Gtk.ResponseType.CANCEL)
        content = dialog.get_content_area()
        checks = {}
        for flag in flags:
            check = Gtk.CheckButton(label=flag)
            check.set_active(flag in current_flags)
            content.append(check)
            checks[flag] = check
        dialog.show()
        response = dialog.run()
        if response == Gtk.ResponseType.OK:
            p["flags"] = [f for f in flags if checks[f].get_active()]
            self._refresh()
        dialog.destroy()

    def _on_write(self, btn):
        self.result = {"partitions": self._partitions, "free_space": self._free_space}
        self._quit()

    @staticmethod
    def _human_size_bytes(s):
        s = s.strip().upper()
        try:
            if s.endswith("TIB") or s.endswith("TB"):
                return float(s[:-3].strip()) * 1024**4
            elif s.endswith("GIB") or s.endswith("GB"):
                return float(s[:-3].strip()) * 1024**3
            elif s.endswith("MIB") or s.endswith("MB"):
                return float(s[:-3].strip()) * 1024**2
            elif s.endswith("KIB") or s.endswith("KB"):
                return float(s[:-3].strip()) * 1024
            else:
                return float(s.rstrip("B").strip() or 0)
        except ValueError:
            return 0.0

    @staticmethod
    def _bytes_human(num):
        for unit in ["B", "KIB", "MIB", "GIB", "TIB"]:
            if abs(num) < 1024.0:
                return f"{num:.1f} {unit}"
            num /= 1024.0
        return f"{num:.1f} PIB"