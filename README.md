# FILLY — Forge Interface Linux Library, Yeah

A unified UI toolkit that renders the same widget as a terminal TUI or a native
GTK4 window using a single JSON protocol.

**One daemon. One core. Two faces.**

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

```text
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
                    └──────────┬──────────┘
                               │
                    ┌──────────▼──────────┐
                    │    filly-daemon     │
                    │  Socket listener    │
                    │  Client multiplex   │
                    │  Session persist    │
                    └──────────┬──────────┘
                               │
                    ┌──────────▼──────────┐
                    │     filly-core      │
                    │  Widget state       │
                    │  Event routing      │
                    │  Focus management   │
                    │  Layout engine      │
                    │  Style/themes       │
                    │  Store (shared KV)  │
                    │  RenderTree         │
                    └──────┬───────┬──────┘
                           │       │
              ┌────────────▼─┐ ┌───▼────────────┐
              │ filly-terminal│ │ filly-graphical│
              │ crossterm     │ │ GTK4           │
              │ ratatui       │ │ libadwaita     │
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
reliably. But it's a collection of independent commands, each spawning a new
process, parsing arguments, rendering, and exiting. It has no persistent
state, no composite widgets, no daemon mode, no layout control, no graphical
backend, and no plugin system. It is the right tool for simple scripts, and
the wrong tool for anything ambitious.

**forge-tui** replaced gum for the ArtixForge installer. It added a daemon,
proper centering, mouse support, a progress bar with stage markers, composite
hub widgets, and a shared state store. It proved that a TUI toolkit could be
fast, beautiful, and functional. But it was tied to the terminal—no graphical
backend, no Python bindings, no plugin system, and the codebase was tightly
coupled to the installer's specific needs.

**forge-gui** was the GTK4 sibling. Same JSON protocol, completely separate
Python codebase. It worked, but maintaining two independent implementations of
every widget was unsustainable.

FILLY is the merger.

It takes the protocol from forge-tui, the widget library from both, and the
lessons learned from building a real installer, and wraps them in a single
Rust core with pluggable backends.

The terminal backend inherits forge-tui's ratatui rendering.

The graphical backend inherits forge-gui's GTK4 windows.

The Bash library is a drop-in replacement for the gum and forge-tui shell
functions.

The Python bindings let you use the same widgets from Python scripts.

> **One library. Two faces. Zero compromises.**
---

## Features

- **33 widget types** — menus, inputs, checklists, file pickers, text editors,
  progress bars, disk partitioners, color pickers, and more
- **Dual backends** — terminal (crossterm + ratatui) and graphical (GTK4 +
  libadwaita) from the same JSON protocol
- **Daemon mode** — persistent Unix socket listener with session state,
  streaming progress, and state subscriptions
- **Plugin system** — load custom widgets from `.so` files at runtime
- **Theme engine** — 8 built-in themes with live reload
- **Client libraries** — Bash, Python, Go, and Node.js
- **Accessibility** — AT-SPI support in the GUI backend, metadata in the
  terminal render tree
- **Clipboard** — paste from system clipboard in all text widgets
- **Binary protocol** — optional MessagePack mode for lower latency

---

## Crate Structure

```text
FILLY/
├── filly-protocol/       # Request/Response types, JSON schema
├── filly-core/           # Widget trait, layout, events, store, themes
├── filly-daemon/         # Unix socket IPC, client handling
├── filly-terminal/       # Crossterm + Ratatui backend
├── filly-graphical/      # GTK4 + libadwaita backend (Python)
├── filly-python/         # Python bindings
├── filly-bin/            # CLI entry point
├── fil.sh                # Single-file Bash library
├── themes/               # Predefined theme files
├── plugins/              # Domain-specific plugin packs
│   ├── artixforge/       # ArtixForge installer widgets
│   └── gforge/           # Gentoo/GForge portage widgets
├── go-filly/             # Go client library
└── node-filly/           # Node.js client library
```

---

## Quick Start

Build the terminal backend:

```bash
cargo build --release
./target/release/filly demo
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