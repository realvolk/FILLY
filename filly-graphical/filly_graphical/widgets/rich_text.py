from .base import BaseWindow
from gi.repository import Gtk

class RichTextWindow(BaseWindow):
    def run(self):
        content = self.params.get("content", "")
        self._window = Gtk.Window(title=self.title_text or "Rich Text")
        self._window.set_default_size(500, 400)
        self._window.connect("destroy", lambda w: self._quit())
        self._window.set_accessible_role("document")

        scrolled = Gtk.ScrolledWindow()
        scrolled.set_hexpand(True); scrolled.set_vexpand(True)

        textview = Gtk.TextView()
        textview.set_editable(False)
        textview.set_wrap_mode(Gtk.WrapMode.WORD)
        textview.set_accessible_label("Rich text content")
        buf = textview.get_buffer()

        import re
        pos = 0
        for match in re.finditer(r'\[([^\]]+)\]\(([^)]+)\)', content):
            before = content[pos:match.start()]
            if before:
                buf.insert(buf.get_end_iter(), before)
            link_text = match.group(1)
            url = match.group(2)
            start_iter = buf.get_end_iter()
            buf.insert(buf.get_end_iter(), link_text)
            end_iter = buf.get_end_iter()
            tag = buf.create_tag(None, foreground="blue", underline=True)
            buf.apply_tag(tag, start_iter, end_iter)
            # Store URL for click handling
            tag.set_data("url", url)
            pos = match.end()
        if pos < len(content):
            buf.insert(buf.get_end_iter(), content[pos:])

        scrolled.set_child(textview)
        self._window.set_child(scrolled)
        self._window.show()
        self.result = None
        self.cancelled = False
        self._quit()
        return {"result": self.result, "cancelled": self.cancelled}