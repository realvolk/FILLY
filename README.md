# FILLY — Forge Interface Linux Library, Yeah

A universal UI server and widget toolkit that renders the same widget on a
terminal, a graphical surface, or an in-memory buffer using a single JSON
protocol.

**One library. Every surface. Zero compromises.**

Designed for operating system installers, system configuration tools, and any
project needing interactive prompts that work identically over SSH, on a raw
TTY, or on a Wayland desktop.

---

## What It Is

FILLY speaks a formal, versioned NDJSON protocol over Unix sockets. You send a
JSON request describing a widget — a menu, a form, a disk partitioner, a hub —
and FILLY renders it on whichever backend is active. The caller never knows or
cares whether the user is on a Linux console, a terminal emulator, or a
graphical desktop. The protocol is identical across all surfaces.

---

## Architecture

```
                    JSON IPC (Unix socket / stdin)
                               │
            ┌──────────────────┼──────────────────┐
            │                  │                  │
       filly send         filly relay        C client lib
            │                  │                  │
            └──────────────────┼──────────────────┘
                               │
                    ┌──────────▼──────────┐
                    │      daemon         │
                    │  Unix socket        │
                    │  Client threads     │
                    │  Session state      │
                    │  Checkpoint/restore │
                    └──────────┬──────────┘
                               │
                    ┌──────────▼──────────┐
                    │       core          │
                    │  Widget vtable      │
                    │  Event loop         │
                    │  RenderTree         │
                    │  Store (KV)         │
                    │  Themes             │
                    │  FIL scripting      │
                    └──────┬───────┬──────┘
                           │       │
              ┌────────────▼─┐ ┌───▼────────────┐
              │   terminal    │ │     gcore      │
              │ ANSI escapes  │ │ DRM/X11/Wayland│
              │ termios       │ │ stb_truetype   │
              │ 24-bit color  │ │ pixel renderer │
              └───────────────┘ └───────┬────────┘
                                        │
                               ┌────────▼────────┐
                               │    headless      │
                               │ in-memory buffer │
                               │ CI testing       │
                               └─────────────────┘
```

---

## Why C, Why Now

**dialog** and **ncurses** work but look like 1995 — no centering, no theming,
no composability.

**gum** is excellent for simple shell scripts but has no persistent state, no
composite widgets, no daemon mode, no graphical backend.

**forge-tui** (Rust) proved a TUI toolkit could be fast and beautiful but was
terminal-only with no plugin system.

**forge-gui** (Python/GTK4) shared the same protocol but was a completely
separate codebase. Maintaining two implementations of every widget was
unsustainable.

FILLY merges both into a single ANSI C codebase with pluggable backends. The
terminal backend uses raw ANSI escapes and termios. The graphical backend
renders through DRM, X11, or Wayland. The Bash client library is a drop-in
replacement for gum and forge-tui shell functions. The daemon handles sessions,
checkpointing, and plugin loading.

> **One library. Every surface. Zero dependencies beyond a C compiler and
> libsodium.**

---

## Features

- **33 widget types** — menus, inputs, checklists, file pickers, text editors,
  progress bars, disk partitioners, color pickers, and more
- **Three backends** — terminal (ANSI + termios), graphical (DRM/X11/Wayland
  via gcore), headless (in-memory buffer for CI)
- **Daemon mode** — persistent Unix socket listener with session checkpointing,
  inactivity timeout, and client threading
- **Plugin system** — load custom widgets from `.so` files; Ed25519 signature
  verification via libsodium
- **Theme engine** — 8 built-in JSON themes with variable resolution, widget
  selectors, and inheritance
- **FIL scripting language** — embedded English-like DSL for validation,
  filtering, conditional visibility, and style definitions
- **Client libraries** — native C client, Bash (`fil.sh`), plus Python and Go
  bindings
- **CLI tools** — `filly send` (one-shot requests), `filly build` (JSON
  constructor), `filly relay` (interactive TTY bridge), `filly oneshot`
  (direct terminal/headless rendering)
- **Test suite** — 119 behavioral tests covering all widgets and plugins, plus
  a native C test binary
- **Styled terminal UI** — single/double/rounded borders, text wrapping,
  scrollable lists, gauge bars, calendar grid, tabbed panes, split views,
  tree indentation, 24-bit true color

---

## Project Structure

```
FILLY/
├── src/
│   ├── core/              # Widget vtable, event loop, RenderTree, store,
│   │   └── widgets/       #   themes, arena, clipboard, undo, relay, client
│   │                      # All 33 built-in widget implementations
│   ├── backend/
│   │   ├── terminal/      # ANSI escape renderer, termios backend
│   │   ├── headless/      # In-memory buffer backend for CI
│   │   ├── gcore/         # DRM/X11/Wayland pixel renderer
│   │   └── daemon/        # Unix socket IPC, checkpoint, verify, sandbox
│   ├── cli/               # main.c — CLI entry point (daemon, oneshot, relay,
│   │                      #   send, build, test)
│   ├── protocol/          # NDJSON framing, schema validation, msgpack
│   ├── script/            # FIL interpreter (fil.c, fil.h)
│   └── themes/            # 8 predefined JSON theme files
├── plugins/
│   ├── artixforge/        # ArtixForge installer widgets (10 widgets)
│   └── gforge/            # Gentoo/GForge portage widgets (7 widgets)
├── test/                  # Behavioral harness, C test binary, unit tests,
│   └── fuzz/              #   pixel renderer test, fuzzer harness
├── tools/                 # genkey, sign, verify — Ed25519 key management
├── Makefile               # Single build system
└── fil.sh                 # Single-file Bash client library
```

---

## Quick Start

```bash
make
./filly oneshot --input test.json
./filly daemon &
./filly send '{"widget":"msg","params":{"title":"Hello","message":"World"}}'
```

Use from Bash:

```bash
source fil.sh
filly_yesno "Test" "Is FILLY working?"
```

---

## License

Licensed under the **Forge Attribution License 1.0**

© Volk 2026

See [LICENSE](LICENSE).