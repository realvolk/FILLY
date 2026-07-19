import sys, json, os, argparse
import gi
gi.require_version('Gtk', '4.0')
gi.require_version('Adw', '1')
from gi.repository import Gtk, Adw
from .backend import GuiBackend

def main():
    Adw.init()
    parser = argparse.ArgumentParser(description="FILLY graphical backend")
    parser.add_argument("--mode", choices=["auto", "gui", "config"], default="auto",
                        help="Display mode: config = persistent configuration window")
    parser.add_argument("--input", "-i", type=argparse.FileType("r"), default=sys.stdin)
    parser.add_argument("--output", "-o", type=argparse.FileType("w"), default=sys.stdout)
    args = parser.parse_args()

    if args.mode == "config":
        state_file = "/tmp/artix-installer/state.conf"
        os.makedirs(os.path.dirname(state_file), exist_ok=True)
        try:
            import importlib.util
            spec = importlib.util.spec_from_file_location(
                "artixforge_config",
                os.path.join(os.environ.get("FILLY_ROOT", ""), "plugins", "artixforge", "python", "app.py")
            )
            if spec and os.path.exists(spec.origin):
                module = importlib.util.module_from_spec(spec)
                spec.loader.exec_module(module)
                module.run_config_mode(state_file)
            else:
                from filly_graphical.artixgui.app import run_config_mode
                run_config_mode(state_file)
        except Exception as e:
            print(f"GUI config mode failed: {e}", file=sys.stderr)
            sys.exit(1)
        sys.exit(0)

    raw = args.input.read()
    data = json.loads(raw) if raw.strip() else {}
    backend = GuiBackend()
    result = backend.run(data)
    json.dump(result, args.output)
    args.output.write("\n")