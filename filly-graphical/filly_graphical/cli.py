import sys, json, os, argparse
import gi
gi.require_version('Gtk', '4.0')
gi.require_version('Adw', '1')
from gi.repository import Gtk, Adw
from .backend import GuiBackend

def main():
    Adw.init()
    parser = argparse.ArgumentParser(description="FILLY graphical backend")
    parser.add_argument("--input", "-i", type=argparse.FileType("r"), default=sys.stdin)
    parser.add_argument("--output", "-o", type=argparse.FileType("w"), default=sys.stdout)
    args = parser.parse_args()
    raw = args.input.read()
    data = json.loads(raw) if raw.strip() else {}
    backend = GuiBackend()
    result = backend.run(data)
    json.dump(result, args.output)
    args.output.write("\n")