import gi
gi.require_version('Gtk', '4.0')
from gi.repository import Gtk, GLib
from .base import BaseWindow

class TreeWindow(BaseWindow):
    def run(self):
        self._nodes = self.params.get("nodes", [])
        self.result = None
        self.cancelled = False

        self._window = Gtk.Window(title=self.title_text or "Tree")
        self._window.set_default_size(500, 400)
        self._window.connect("destroy", lambda w: self._quit())
        self._window.set_accessible_role("dialog")

        vbox = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=10)
        vbox.set_margin_top(10); vbox.set_margin_bottom(10)
        vbox.set_margin_start(10); vbox.set_margin_end(10)
        self._window.set_child(vbox)

        title_label = Gtk.Label(label=self.title_text)
        title_label.add_css_class("title-1")
        vbox.append(title_label)

        scrolled = Gtk.ScrolledWindow()
        scrolled.set_hexpand(True); scrolled.set_vexpand(True)
        self._tree_listbox = Gtk.ListBox()
        self._tree_listbox.set_selection_mode(Gtk.SelectionMode.SINGLE)
        self._tree_listbox.connect("row-activated", self._on_row_activated)
        self._tree_listbox.set_accessible_role("tree")
        self._tree_listbox.set_accessible_label("File tree")
        scrolled.set_child(self._tree_listbox)
        vbox.append(scrolled)

        self._flat_nodes = []
        self._build_flat_list(self._nodes, 0)
        self._rebuild_ui()

        btn_box = Gtk.Box(spacing=10)
        btn_box.set_halign(Gtk.Align.CENTER)
        cancel_btn = Gtk.Button(label="Cancel")
        cancel_btn.connect("clicked", self._cancel)
        btn_box.append(cancel_btn)
        vbox.append(btn_box)

        self._window.show()
        loop = GLib.MainLoop()
        self._window.connect("destroy", lambda *_: loop.quit())
        loop.run()
        return {"result": self.result, "cancelled": self.cancelled}

    def _build_flat_list(self, nodes, depth):
        for node in nodes:
            self._flat_nodes.append((depth, node))
            if node.get("expanded", False):
                self._build_flat_list(node.get("children", []), depth + 1)

    def _rebuild_ui(self):
        while True:
            row = self._tree_listbox.get_row_at_index(0)
            if not row: break
            self._tree_listbox.remove(row)

        for depth, node in self._flat_nodes:
            row = Gtk.ListBoxRow()
            hbox = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=5)
            indent = Gtk.Label(label="  " * depth)
            hbox.append(indent)

            children = node.get("children", [])
            if children:
                expander = Gtk.Button(label="▼" if node.get("expanded", False) else "▶")
                expander.set_has_frame(False)
                expander.connect("clicked", self._make_expand_handler(node))
                expander.set_accessible_label(
                    f"{'Collapse' if node.get('expanded', False) else 'Expand'} {node.get('label', '')}"
                )
                hbox.append(expander)
            else:
                hbox.append(Gtk.Label(label="  "))

            lbl = Gtk.Label(label=node.get("label", ""), xalign=0)
            lbl.set_accessible_label(node.get("label", ""))
            hbox.append(lbl)
            row.set_child(hbox)
            row._node = node
            self._tree_listbox.append(row)

    def _make_expand_handler(self, node):
        def handler(btn):
            node["expanded"] = not node.get("expanded", False)
            self._flat_nodes = []
            self._build_flat_list(self._nodes, 0)
            self._rebuild_ui()
        return handler

    def _on_row_activated(self, listbox, row):
        if hasattr(row, '_node'):
            self.result = row._node.get("label", "")
            self._quit()

    def _cancel(self, btn=None):
        self.cancelled = True
        self._quit()