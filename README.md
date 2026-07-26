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

FILLY speaks a formal, versioned NDJSON (or MessagePack) protocol over Unix
sockets. You send a JSON request describing a widget — a menu, a form, a disk
partitioner, a hub — and FILLY renders it on whichever backend is active.
The caller never knows or cares whether the user is on a Linux console, a
terminal emulator, or a graphical desktop. The protocol is identical across
all surfaces.

---

## Architecture

```
                    JSON / MessagePack IPC (Unix socket / stdin)
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
                    │  Device profiles    │
                    │  MessagePack codec  │
                    └──────────┬──────────┘
                               │
                    ┌──────────▼──────────┐
                    │       core          │
                    │  Widget vtable      │
                    │  Event loop         │
                    │  RenderTree         │
                    │  Store (KV)         │
                    │  Themes             │
                    │  Animation engine   │
                    │  FIL scripting      │
                    │  Recorder           │
                    └──────┬───────┬──────┘
                           │       │
              ┌────────────▼─┐ ┌───▼────────────┐
              │   terminal    │ │     gcore      │
              │ ANSI escapes  │ │ DRM/X11/Wayland│
              │ termios       │ │ stb_truetype   │
              │ 24-bit color  │ │ pixel renderer │
              │ incremental   │ │ glyph cache    │
              │ TUI animation │ │ GPU transforms │
              └───────────────┘ └───────┬────────┘
                                        │
                               ┌────────▼────────┐
                               │    headless      │
                               │ in-memory buffer │
                               │ pixel buffer     │
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

- **36 widget types** — menus, inputs, checklists, file pickers, text editors,
  progress bars, disk partitioners, color pickers, terminal emulator, widget
  builder, macro recorder, and more
- **Four backends** — terminal (ANSI + termios), graphical (DRM/X11/Wayland
  via gcore), headless (in-memory buffer for CI), headless pixel
- **Dual wire formats** — NDJSON (default) and MessagePack (negotiated)
- **Daemon mode** — persistent Unix socket listener with session checkpointing,
  inactivity timeout, client threading, rate limiting, device profile detection
- **Plugin system** — load custom widgets from `.so` files; Ed25519 signature
  verification via libsodium; sandboxed execution (seccomp/pledge/capsicum)
- **Theme engine** — 8 built-in JSON themes with variable resolution, colour
  arithmetic functions, widget selectors, inheritance, transitions, animations,
  and live reload via inotify/kqueue
- **Animation engine** — keyframe-based with 6 easing functions, per-property
  interpolation, FIL animation control, GPU transforms (scale/rotate/translate),
  TUI-safe animation subset
- **GUI builder** — `filly-build` visual widget composer with canvas, connection
  graph editor, dynamic property editor, validation pipeline, and C code
  generation for compilable plugin output
- **FIL scripting language** — embedded English-like DSL for validation,
  filtering, conditional visibility, style definitions, animation control,
  and keybindings
- **Client libraries** — native C client, Bash (`fil.sh`), Python, Go, Node.js
- **CLI tools** — `filly send`, `filly build`, `filly relay`, `filly oneshot`,
  `filly compile`, `filly update`, `filly-build`
- **Test suite** — 119 behavioral tests, 14 C tests, fuzzer, fault injection,
  performance benchmarks, snapshot testing (pixel + ANSI), Valgrind CI
- **Portability** — Linux, FreeBSD, OpenBSD via `filly-port/` abstraction layer
- **Shared memory IPC** — 16 MB POSIX shared memory for zero-copy GUI frame transfer
- **Input methods** — keyboard, mouse, touch, gamepad (via libinput)
- **Styled terminal UI** — single/double/rounded borders, text wrapping,
  scrollable lists, gauge bars, calendar grid, tabbed panes, split views,
  tree indentation, 24-bit true color
- **Macro recording** — record and replay sessions with frame-accurate comparison
- **Self-updating** — `filly update` downloads and execs into latest release

---

## Quick Start

```bash
make
./filly oneshot --input test.json
./filly daemon &
./filly send '{"widget":"msg","params":{"title":"Hello","message":"World"}}'
./filly-build  # launch the GUI builder
```

Use from Bash:

```bash
source fil.sh
filly_yesno "Test" "Is FILLY working?"
```

Use from Python:

```python
from filly import Client
c = Client()
c.send('{"widget":"msg","params":{"title":"Hello","message":"World"}}')
```

---

## Project Structure

```
FILLY/
├── src/
│   ├── core/              # Widget vtable, event loop, RenderTree, store,
│   │   └── widgets/       #   themes, arena, clipboard, undo, relay, client,
│   │                      #   recorder, animation engine. 36 widgets
│   ├── builder/           # GUI builder (project model, canvas, graph editor,
│   │                      #   property editor, codegen, validator)
│   ├── backend/
│   │   ├── terminal/      # ANSI escape renderer, termios backend
│   │   ├── headless/      # In-memory buffer backend for CI
│   │   ├── gcore/         # DRM/X11/Wayland pixel renderer + GPU
│   │   └── daemon/        # Unix socket IPC, checkpoint, verify, sandbox
│   ├── cli/               # main.c — CLI entry point
│   ├── protocol/          # NDJSON framing, schema validation, msgpack
│   ├── script/            # FIL interpreter (with animation statements)
│   ├── filly-port/        # Platform abstraction (Linux/FreeBSD/OpenBSD)
│   └── themes/            # 8 predefined JSON theme files
├── plugins/
│   ├── artixforge/        # ArtixForge installer widgets (10 widgets)
│   └── gforge/            # Gentoo/GForge portage widgets (7 widgets)
├── bindings/
│   ├── python/            # Python ctypes bindings
│   ├── go/                # Go cgo bindings
│   └── node/              # Node.js napi bindings
├── test/                  # Behavioral harness, C test binary, unit tests,
│   └── fuzz/              #   pixel test, snapshot, fuzzer, fault inject,
│                          #   benchmark
├── tools/                 # genkey, sign, verify
├── po/                    # Translation template
├── .github/workflows/     # CI: valgrind, snapshot, lint, cppcheck, benchmark,
│                          #   fault-inject, fuzz, freebsd, openbsd
├── Makefile               # Single build system
└── fil.sh                 # Single-file Bash client library
```

---

## License

Licensed under the **Forge Attribution License 1.0**

© Volk 2026

See [LICENSE](LICENSE).