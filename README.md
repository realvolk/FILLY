# FILLY — Forge Interface Linux Library, Yeah

A unified UI toolkit that renders the same widget as a terminal TUI or a native
GTK4 window using a single JSON protocol.

**One library. Two faces. Zero compromises.**

Designed for Linux installers, CLI tools, and any project needing interactive
prompts that work transparently over SSH, in a terminal, or on a desktop.

---

## What It Is

FILLY is a widget library with multiple backends.

You send a JSON request describing what you want—a menu, a yes/no prompt, a
text input, or another widget—and FILLY renders it using whichever backend is
currently active.

Whether the user is on:

- a Linux TTY
- a terminal emulator
- an SSH session
- a Wayland or X11 desktop

they receive an interface that feels native to their environment.

The caller doesn't know or care which backend is active.

---

## Architecture

```
                        JSON IPC (Unix socket / stdin)
                               │
┌──────────────────────────────┼──────────────────────────────┐
│                              │                              │
│  Bash (fil.sh)          Python (filly)            Go (go-filly)
│                              │                              │
└──────────────────────────────┼──────────────────────────────┘
                               │
                    ┌──────────▼──────────┐
                    │   filly-protocol    │
                    │  Request / Response │
                    │      (cJSON)        │
                    └──────────┬──────────┘
                               │
                    ┌──────────▼──────────┐
                    │    filly-daemon     │
                    │  Unix socket        │
                    │  Client threads     │
                    │  Session state      │
                    └──────────┬──────────┘
                               │
                    ┌──────────▼──────────┐
                    │     filly-core      │
                    │  Widget vtable      │
                    │  Event loop         │
                    │  RenderTree         │
                    │  Store (KV)         │
                    │  Themes             │
                    └──────┬───────┬──────┘
                           │       │
              ┌────────────▼─┐ ┌───▼────────────┐
              │ filly-terminal│ │ filly-graphical│
              │ ANSI escapes  │ │ GTK4           │
              │ termios       │ │ libadwaita     │
              │ 256-color     │ │ (Python)       │
              └───────────────┘ └────────────────┘
```

---

## Why Did You Even Make This?

Because the existing options all hit a wall.

**dialog** and **ncurses** are the old guard. They work, but they look like
1995. No centering, no theming, no composability, no mouse support worth
using. You can't build a modern installer with `dialog`—you can only chain it
and hope the user squints.

**gum** is a well-executed tool for shell scripts that need occasional
interactive prompts. It's prettier than dialog, integrates cleanly, and works
reliably. But it's a collection of independent commands with no persistent
state, no composite widgets, no daemon mode, and no graphical backend. It is
the right tool for simple scripts, and the wrong tool for anything ambitious.

**forge-tui** replaced gum for the ArtixForge installer. It added a daemon,
proper centering, mouse support, progress bars with stage markers, composite
hub widgets, and a shared state store. It proved a TUI toolkit could be fast,
beautiful, and functional. But it was Rust-only, terminal-only, with no Python
bindings and no plugin system.

**forge-gui** was the GTK4 sibling. Same JSON protocol, completely separate
Python codebase. Maintaining two independent implementations of every widget
was unsustainable.

FILLY is the merger — and now entirely in C.

It takes the protocol from forge-tui, the widget library from both, and the
lessons learned from building a real installer, and wraps them in a single
ANSI C core with pluggable backends.

The terminal backend uses raw ANSI escapes and termios — no Rust, no ratatui,
no crossterm. Just C and a terminal.

The graphical backend inherits forge-gui's GTK4 windows — unchanged.

The Bash library is a drop-in replacement for the gum and forge-tui shell
functions.

> **One library. Two faces. Zero dependencies beyond a C compiler.**
---

## Features

- **33 widget types** — menus, inputs, checklists, file pickers, text editors,
  progress bars, disk partitioners, color pickers, and more
- **Dual backends** — terminal (ANSI C + termios) and graphical (GTK4 +
  libadwaita, Python) from the same JSON protocol
- **Daemon mode** — persistent Unix socket listener with session state and
  client threading
- **Plugin system** — load custom widgets from `.so` files at runtime via
  `dlopen`/`dlsym`
- **Theme engine** — 8 built-in themes with JSON stylesheets
- **Client libraries** — Bash, Python, Go, and Node.js
- **Accessibility** — AT-SPI support in the GUI backend, metadata in the
  terminal render tree
- **Styled terminal UI** — single/double/rounded borders, text wrapping with
  word break, scrollable lists, gauge bars, calendar grid, tabbed panes,
  split views, tree indentation, 256-color support
- **Zero Rust dependencies** — pure C, buildable with `gcc` and `make`

---

## Project Structure

```
FILLY/
├── filly-protocol/       # Request/Response types, JSON parse/serialize
├── filly-core/           # Widget vtable, event loop, RenderTree, store, themes
├── filly-daemon/         # Unix socket IPC, client threads
├── filly-terminal/       # ANSI escape renderer, termios backend
├── filly-graphical/      # GTK4 + libadwaita backend (Python, unchanged)
├── filly-python/         # Python bindings
├── filly-bin/            # CLI entry point (main.c)
├── cJSON.c/h             # Vendored JSON parser
├── Makefile              # Build system
├── fil.sh                # Single-file Bash library
├── themes/               # Predefined theme files
├── plugins/              # Domain-specific plugin packs
│   ├── artixforge/       # ArtixForge installer widgets (C + Python)
│   └── gforge/           # Gentoo/GForge portage widgets (C + Python)
├── go-filly/             # Go client library
└── node-filly/           # Node.js client library
```

---

## Quick Start

Build the terminal backend:

```bash
make
./filly demo
```

Use from Bash:

```bash
source fil.sh
filly_yesno "Test" "Is FILLY working?"
```

Use from Python:

```python
import filly
with filly.Session() as f:
    print(f.menu("Title", "Message", ["A", "B", "C"]))
```

---

## License

Licensed under the **Forge Attribution License 1.0**

© Volk 2026

See [LICENSE](LICENSE).