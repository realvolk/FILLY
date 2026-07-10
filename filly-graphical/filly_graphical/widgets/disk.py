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

        self._window = Gtk.Window(title=f"{self.title_text} ({self._disk})")
        self._window.set_default_size(900, 600)
        self._window.connect("destroy", lambda w: self._quit())
        self._window.set_accessible_role("dialog")

        vbox = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=10)
        vbox.set_margin_top(10); vbox.set_margin_bottom(10)
        vbox.set_margin_start(10); vbox.set_margin_end(10)
        self._window.set_child(vbox)

        # Visual partition bar
        self._bar_drawing = Gtk.DrawingArea()
        self._bar_drawing.set_content_height(40)
        self._bar_drawing.set_draw_func(self._draw_partition_bar)
        self._bar_drawing.set_accessible_label("Disk partition layout")
        vbox.append(self._bar_drawing)

        # Partition list
        scrolled = Gtk.ScrolledWindow()
        scrolled.set_hexpand(True); scrolled.set_vexpand(True)
        self._listbox = Gtk.ListBox()
        self._listbox.set_selection_mode(Gtk.SelectionMode.SINGLE)
        self._listbox.connect("row-selected", self._on_row_selected)
        self._listbox.set_accessible_role("list")
        self._listbox.set_accessible_label("Partitions and free space")
        scrolled.set_child(self._listbox)
        vbox.append(scrolled)

        # Detail label
        self._detail_label = Gtk.Label()
        self._detail_label.set_xalign(0)
        self._detail_label.set_accessible_role("status")
        vbox.append(self._detail_label)

        # Action buttons
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
                btn.set_accessible_label(f"{label} partition action")
                btn_box.append(btn)
            vbox.append(btn_box)

        close_btn = Gtk.Button(label="Close")
        close_btn.connect("clicked", lambda b: self._quit())
        vbox.append(close_btn)

        self._refresh()
        self._window.show()
        loop = GLib.MainLoop()
        self._window.connect("destroy", lambda *_: loop.quit())
        loop.run()
        return {"result": self.result, "cancelled": self.cancelled}

    def _refresh(self):
        while True:
            row = self._listbox.get_row_at_index(0)
            if not row: break
            self._listbox.remove(row)

        for i, p in enumerate(self._partitions):
            row = Gtk.ListBoxRow()
            label = Gtk.Label(
                label=f"  {p.get('number', '')}  {p.get('size', '')}  {p.get('type', '')}",
                xalign=0
            )
            if i == self._selected:
                label.set_markup(f"<b>  {p.get('number', '')}  {p.get('size', '')}  {p.get('type', '')}</b>")
            label.set_accessible_label(f"Partition {p.get('number', '')}: {p.get('size', '')}, {p.get('type', '')}")
            row.set_child(label)
            self._listbox.append(row)

        for i, fs in enumerate(self._free_space):
            idx = len(self._partitions) + i
            row = Gtk.ListBoxRow()
            label = Gtk.Label(
                label=f"       {fs.get('size', '')}  Free space",
                xalign=0
            )
            if idx == self._selected:
                label.set_markup(f"<b>       {fs.get('size', '')}  Free space</b>")
            label.set_accessible_label(f"Free space: {fs.get('size', '')}")
            row.set_child(label)
            self._listbox.append(row)

        self._bar_drawing.queue_draw()

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
        total = len(self._partitions)
        if self._selected < total:
            p = self._partitions[self._selected]
            self._detail_label.set_text(
                f"Partition {p.get('number','')}  Type: {p.get('type','')}  "
                f"Size: {p.get('size','')}  FS: {p.get('fs_signature','none')}  "
                f"Flags: {', '.join(p.get('flags',[])) or 'none'}"
            )
        else:
            fs = self._free_space[self._selected - total]
            self._detail_label.set_text(f"Free space  Size: {fs.get('size','')}")

    def _on_new(self, btn): self.result = {"_action": "new"}; self._quit()
    def _on_delete(self, btn): self.result = {"_action": "delete"}; self._quit()
    def _on_resize(self, btn): self.result = {"_action": "resize"}; self._quit()
    def _on_type(self, btn): self.result = {"_action": "type"}; self._quit()
    def _on_flags(self, btn): self.result = {"_action": "flags"}; self._quit()
    def _on_write(self, btn):
        self.result = {"partitions": self._partitions, "free_space": self._free_space}
        self._quit()