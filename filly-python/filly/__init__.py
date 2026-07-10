import subprocess
import json
import os
import socket

class Session:
    def __init__(self, socket_path="/tmp/filly.sock"):
        self.socket_path = socket_path
        self.process = None
        if not os.path.exists(socket_path):
            self.process = subprocess.Popen(["filly", "daemon", "--socket", socket_path])

    def __enter__(self):
        return self

    def __exit__(self, *args):
        if self.process:
            self.process.terminate()

    def _send(self, widget, params=None):
        if params is None:
            params = {}
        req = {"widget": widget, "params": params}
        sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        sock.connect(self.socket_path)
        sock.sendall(json.dumps(req).encode() + b"\n")
        data = b""
        while True:
            chunk = sock.recv(4096)
            if not chunk:
                break
            data += chunk
            if b"\n" in data:
                break
        sock.close()
        return json.loads(data.decode().strip())

    def menu(self, title, message, choices, default=None):
        params = {"title": title, "message": message, "choices": choices}
        if default is not None:
            params["default"] = default
        return self._send("menu", params)

    def yesno(self, title, message, default=None):
        params = {"title": title, "message": message}
        if default is not None:
            params["default"] = default
        return self._send("yesno", params)

    def input(self, title, message, default=None, placeholder=None, validation=None):
        params = {"title": title, "message": message}
        if default is not None:
            params["default"] = default
        if placeholder is not None:
            params["placeholder"] = placeholder
        if validation is not None:
            params["validation"] = validation
        return self._send("input", params)

    def password(self, title, message, placeholder=None):
        params = {"title": title, "message": message}
        if placeholder is not None:
            params["placeholder"] = placeholder
        return self._send("password", params)

    def checklist(self, title, message, choices, min_selected=0, max_selected=None, default=None):
        params = {"title": title, "message": message, "choices": choices}
        if min_selected:
            params["min"] = min_selected
        if max_selected is not None:
            params["max"] = max_selected
        if default is not None:
            params["default"] = default
        return self._send("checklist", params)

    def msg(self, title, message):
        return self._send("msg", {"title": title, "message": message})

    def progress(self, title, command):
        return self._send("progress", {"title": title, "command": command})

    def file_picker(self, title, start_dir="/", filter_ext=""):
        return self._send("file_picker", {"title": title, "start_dir": start_dir, "filter": filter_ext})

    def filter(self, title, message, choices, placeholder=""):
        return self._send("filter", {"title": title, "message": message, "choices": choices, "placeholder": placeholder})

    def multiselect(self, title, message, choices, placeholder="", min_selected=0, max_selected=None):
        params = {"title": title, "message": message, "choices": choices, "placeholder": placeholder}
        if min_selected:
            params["min"] = min_selected
        if max_selected is not None:
            params["max"] = max_selected
        return self._send("multiselect", params)

    def summary(self, title, message=None, file_path=None):
        params = {"title": title}
        if message:
            params["message"] = message
        if file_path:
            params["file"] = file_path
        return self._send("summary", params)

    def text_editor(self, title, file_path=None, content=None):
        params = {"title": title}
        if file_path:
            params["file"] = file_path
        if content is not None:
            params["content"] = content
        return self._send("text", params)

    def hub(self, title, categories, actions):
        return self._send("hub", {"title": title, "categories": categories, "actions": actions})

    def button(self, title, label):
        return self._send("button", {"title": title, "label": label})

    def toggle(self, title, label, default=False):
        return self._send("toggle", {"title": title, "label": label, "default": default})

    def spinner(self, message):
        return self._send("spinner", {"message": message})