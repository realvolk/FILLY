# FILLY Specification — v1.0.0

**FILLY** is a universal UI server for operating system deployment, configuration,
and any interactive terminal or graphical workflow. It speaks a formal,
versioned protocol over Unix sockets, renders to terminals, graphical surfaces,
and in-memory buffers. FILLY ships its own C client library, visual GUI builder,
command-line tools, animation engine, and plugin system. The core library fits in
approximately 22,000 lines of C with zero external runtime dependencies beyond
libsodium.

---

## Table of Contents

1. [Protocol Fundamentals](#1-protocol-fundamentals)
2. [Message Catalog](#2-message-catalog)
3. [Daemon Lifecycle & Resilience](#3-daemon-lifecycle--resilience)
4. [Backends](#4-backends)
5. [Widget System](#5-widget-system)
6. [Store & Reactive State](#6-store--reactive-state)
7. [Sessions](#7-sessions)
8. [Plugin System](#8-plugin-system)
9. [Themes & Style Engine](#9-themes--style-engine)
10. [Animation Engine](#10-animation-engine)
11. [GUI Builder](#11-gui-builder)
12. [Accessibility & Internationalisation](#12-accessibility--internationalisation)
13. [Tooling & Correctness](#13-tooling--correctness)
14. [Client Libraries, Tools & Shell Wrappers](#14-client-libraries-tools--shell-wrappers)
15. [The FIL Scripting Language](#15-the-fil-scripting-language)
16. [Security Model](#16-security-model)
17. [Portability](#17-portability)
18. [Future-Proofing & Roadmap](#18-future-proofing--roadmap)

---

## 1. Protocol Fundamentals

FILLY uses **newline-delimited JSON (NDJSON)** as the primary wire format.
MessagePack encoding is fully implemented and activated as a negotiated wire
format via the handshake.

### 1.1 Transport

- **Unix socket** — primary transport, bound to `$XDG_RUNTIME_DIR/filly.sock`
  with `0600` permissions (falls back to `/tmp/filly.sock`).
- **Standard I/O** — oneshot mode reads a single request from `stdin` and
  writes the response to `stdout`. Falls back to headless backend when no
  terminal is available.
- **Relay mode** — a relay client opens `/dev/tty`, sets raw mode, forwards
  draw frames to the TTY, and sends keystrokes to the daemon as JSON messages
  over the same socket connection.

### 1.2 Handshake & Encoding Negotiation

The first message sent by a client **may** be a handshake. If no handshake
is received, the daemon assumes NDJSON. Supported encodings: `json` (default),
`msgpack` (fully implemented and activated).

```json
{"type":"handshake","version":1,"encoding":"msgpack"}
```

When MessagePack is negotiated, all responses from the daemon are encoded
as MessagePack binary. The C client library handles transparent decoding.

### 1.3 Schema Validation

Every incoming message is validated against a JSON Schema before dispatch.
Unknown fields are rejected; malformed messages receive a detailed error
response. Implemented in `protocol/schema.c`.

### 1.4 Message Framing

All messages are single-line JSON objects terminated by `\n`. Binary payloads
(such as draw data) are preceded by a JSON header containing a `len` field,
followed by exactly `len` raw bytes and a trailing newline. The C client
library handles draw-frame reassembly.

---

## 2. Message Catalog

Every message carries a `"type"` field. Widget requests are backward-compatible:
a message without a `"type"` but with a `"widget"` field is treated as a widget
request.

### 2.1 Lifecycle Messages

| type | Direction | Purpose |
|------|-----------|---------|
| `handshake` | Client→Daemon | Negotiate encoding |
| `ping` | Client→Daemon | Keep-alive; daemon replies `pong` |
| `pong` | Daemon→Client | Response to `ping` |
| `quit` | Client→Daemon | Graceful shutdown |
| `session` | Client→Daemon | Create, attach, destroy a persistent session |
| `subscribe` | Client→Daemon | Subscribe to store key changes |
| `unsubscribe` | Client→Daemon | Remove a subscription |
| `state` | Daemon→Client | Push notification of a store change |
| `reload_theme` | Client→Daemon | Reload theme and style files at runtime |
| `reload_plugins` | Client→Daemon | Reload plugins |
| `set_accessibility` | Client→Daemon | Activate an accessibility profile |

### 2.2 Rendering Messages

| type | Direction | Purpose |
|------|-----------|---------|
| `draw` | Daemon→Client | Frame data follows (length-prefixed) |
| `yield` | Daemon→Client | Intermediate progress update |
| `response` | Daemon→Client | Final result of a widget request |

### 2.3 Widget Requests (Backward Compatible)

No `"type"` field; presence of `"widget"` identifies it. Fields `relay`,
`headless`, `gui`, `session_id`, and `tty` are all supported.

### 2.4 Input Messages

| type | Direction | Purpose |
|------|-----------|---------|
| `key` | Client→Daemon | Keystroke injection (`code` and `ch` fields) |
| `mouse` | Client→Daemon | Mouse event injection (`x`, `y`, `button`, `state`) |
| `resize` | Client→Daemon | Terminal / window resize (`w`, `h`) |

---

## 3. Daemon Lifecycle & Resilience

### 3.1 Startup

- Detects device profile (SSH, local TTY, Wayland, X11, headless) and adapts
  theme defaults accordingly.
- Loads plugins from `$HOME/.config/filly/plugins/`.
- Loads themes and animation definitions from the `themes/` directory at startup.
- Binds to Unix socket at `$XDG_RUNTIME_DIR/filly.sock` (`0600`), falling back
  to `/tmp/filly.sock`.
- Redirects stdin/stdout/stderr to `/dev/null` after binding.
- Validates plugin Ed25519 signatures; unsigned plugins rejected unless
  `--insecure-plugins` is set.
- Restores checkpointed sessions from previous run.
- Creates shared memory region for IPC (`/filly_shm`, 16 MB).
- Applies platform sandboxing (seccomp/pledge/capsicum).
- Enters accept loop.

### 3.2 Connection Handling

- Each connection is a separate thread.
- Peer credential verification via `filly-port/` abstraction (Linux
  `SO_PEERCRED`, FreeBSD `LOCAL_PEERCRED`, OpenBSD `getpeereid`).
- Inactivity timeout: 30 seconds.
- Session isolation: sessions scoped to connecting user's UID.
- Rate limiting: configurable max connections per second.

### 3.3 Crash Resilience

Periodic checkpoint of active sessions to `~/.cache/filly/checkpoint.json`
(`0600`). Sensitive keys (pass, LUKS, token, key, secret) excluded from
serialization. Restored on daemon restart.

### 3.4 Self-Restart

Sending `SIGHUP` to the daemon triggers a self-restart via `execve`,
preserving the open socket file descriptor so existing connections are
not dropped. Linux uses `/proc/self/exe`; BSD platforms use `argv[0]` fallback.

### 3.5 Shared Memory IPC

A 16 MB POSIX shared memory region (`/filly_shm`) is created at daemon
startup for zero-copy data transfer between the daemon and GUI clients.

---

## 4. Backends

FILLY's `BackendVTable` abstraction allows the same widget code to render to
any output surface. Backends are selected at runtime.

### 4.1 Terminal Backend

- Raw ANSI escape sequences, 24-bit true colour.
- Synchronised output via `\033[?2026h`.
- Incremental rendering with per-row diffing against previous frame buffer.
- Mouse support via SGR extended coordinates.
- Clipboard copy via OSC 52. Paste from system clipboard via session-level
  Ctrl+V handling.
- TUI animation subset: opacity dimming, border colour cycling.

### 4.2 `filly-gcore` — Native Graphical Backend

**Pixel Renderer:**
- Text rendering via stb_truetype with 256-entry glyph cache.
- Font fallback: DejaVu Sans, Liberation Sans, Noto Sans CJK.
- Full `WidgetStyle` support (colours, borders, rounded corners, padding,
  margins, alignment, shadows, gradients, opacity, transforms).
- Per-node dirty flag with damage-region tracking for incremental rendering.
- GPU acceleration via EGL/OpenGL ES scaffolded; falls back to CPU pixel buffer.
- Full animation interpolation: scale, rotation, translation, colour, shadow,
  gradient, border, opacity, font size.
- Keyframe-based animation engine with 6 easing functions.

**Output Targets:**

| Target | Status |
|--------|--------|
| DRM/KMS | Implemented |
| X11 | Implemented |
| Wayland | Implemented |
| Headless Pixel | Implemented |

**Input Handling:**
- Keyboard, mouse, touch, and gamepad events via libinput.
- Touch-to-click synthesis via RenderTree hit-testing.
- Gamepad button-to-key mapping and analog stick-to-arrow-key conversion.

**Graceful Degradation:**
Probes Wayland → X11 → DRM → terminal TUI at startup.

**Profiling Overlay:**
FPS counter and arena memory usage when compiled with `-DFILLY_PROFILING`.

### 4.3 Headless Backend

Renders to in-memory character and pixel buffers. Accepts synthetic key events.
Used for automated testing. Pixel buffer mode available via
`headless_pixel_vtable`.

### 4.4 Multi-Surface Rendering

`handle_gui_client` creates an array of backends (GCore + optional Terminal
fallback) and passes them to `session_run_multi`.

---

## 5. Widget System

FILLY provides 36 widget types, fully backend-agnostic. Widgets declare
**what** content they contain and **how** it should be structured; backends
decide **where** and **how large** each element appears based on the output
surface. A single widget implementation works identically on a terminal,
a graphical window, or an in-memory buffer.

### 5.1 Content-Only Widget API

Widgets no longer calculate pixel or character positions. Instead they populate
a `RenderTree` with content nodes and **layout hints**. The active backend
performs a layout pass before rendering, assigning final coordinates and
dimensions to every node.

**Content nodes** carry:
- `text.content` for `RNODE_TEXT`
- `list.items` and `list.item_count` for `RNODE_LIST`
- `input.text`, `input.placeholder`, `input.cursor`, `input.masked` for `RNODE_INPUT`
- `checkbox.label`, `checkbox.checked` for `RNODE_CHECKBOX`
- `toggle.label`, `toggle.value` for `RNODE_TOGGLE`
- `badge.text` for `RNODE_BADGE`
- `gauge.percent`, `gauge.label` for `RNODE_GAUGE`
- `calendar.year`, `calendar.month`, `calendar.selected_day` for `RNODE_CALENDAR`
- `table.headers`, `table.rows`, `table.selected_row` for `RNODE_TABLE`
- `tree.nodes`, `tree.node_count` for `RNODE_TREE`
- `form.fields`, `form.submit_label` for `RNODE_FORM`
- `spinner.message`, `spinner.frame` for `RNODE_SPINNER`
- `toast.message` for `RNODE_TOAST`
- `context_menu.items`, `context_menu.selected` for `RNODE_CONTEXT_MENU`
- Container children for `RNODE_CONTAINER`, `RNODE_TABS`, `RNODE_SPLIT_PANES`

**Layout hints** are set via the existing `WidgetStyle` fields, already resolved
by the theme engine before layout:
- `min_width`, `min_height` — size floors, respected by all backends
- `max_width`, `max_height` — size ceilings
- `text_align` — ALIGN_LEFT, ALIGN_CENTER, or ALIGN_RIGHT
- `padding[4]` — top, right, bottom, left padding
- `margin[4]` — top, right, bottom, left margin
- `font_size` — used by graphical backend for text measurement

Widgets may also set:
- `weight` — relative growth factor for flex-like distribution in
  `RNODE_CONTAINER` children (1.0 = default, 0.0 = fixed size)
- `border_style` — NONE, SINGLE, DOUBLE, or ROUNDED for containers

### 5.2 Backend Layout Pass

Every backend implements a layout function:

```
void backend_layout(RenderTree *tree, int surface_w, int surface_h);
```

The layout pass walks the tree top-down and computes final `rect` values
(`x`, `y`, `w`, `h`) for every node based on:

1. **Content measurement** — text length × font metrics (graphical) or
   character count (terminal) determines minimum width/height.
2. **Layout hints** — `min_width`, `min_height`, `max_width`, `max_height`,
   `weight`, and `text_align` constrain or expand nodes.
3. **Container rules** — `RNODE_CONTAINER` distributes available space among
   children according to their `weight` values. `RNODE_TABS` positions tab
   headers and allocates remaining space to the active child. `RNODE_SPLIT_PANES`
   divides space at the split position.
4. **Surface constraints** — nodes are clamped to the available surface area.

**Terminal layout** produces coordinates in character-cell units (columns × rows),
where a "cell" is 1 character wide and 1 character tall. Text is measured in
characters. List items occupy 1 row each.

**Graphical layout** produces coordinates in pixel units, using the active
theme's `font_size` and `padding` values for measurement. Text is measured
in pixels via `stbtt_ScaleForPixelHeight` and glyph advance widths. List items
occupy `font_size + padding_top + padding_bottom` pixels each.

**Headless layout** mirrors the terminal layout in character mode, or the
graphical layout in pixel mode, depending on the `headless_pixel_vtable`
selection.

### 5.3 Widget Composition

Widgets may contain child widgets recursively via `tabs` and `split_panes`
(with three-pane support via `split_panes.third`). The install hub widget
supports sub-widget dispatch for inline editing.

### 5.4 Embedded FIL Scripting

FIL scripts wired to widget validation (with store access), style definitions,
animation control, and keybinding maps.

### 5.5 Widget Lifecycle & State Machine

`EVENT_RESULT_RESPONSE` and `EVENT_RESULT_HANDLED` implemented.

### 5.6 Undo/Redo Stack

Implemented. `Ctrl+Z` / `Ctrl+Y` handled at session level.

### 5.7 Special Widgets

**Terminal Emulator Widget** — embeds a PTY in a widget with scrollback buffer
(64KB), resize handling via TIOCSWINSZ, and `/` search filter. Renders output
as a character grid in both terminal and graphical backends.

**Widget Builder Widget** — visual palette-based widget composer.
Keyboard-driven (F1/F2/F3 mode switching, arrow navigation, S to save).
Exports composed layout as JSON. Uses FILLY's own widget system for its UI.

**Macro Recorder Widget** — UI for recording, playback, save, and load of
session macros. Wraps the recorder subsystem. Supports R/S/P/L/W hotkeys.

### 5.8 Complete Widget List

| # | Widget | Type | Description |
|---|--------|------|-------------|
| 1 | badge | Display | Small coloured label with text |
| 2 | calendar | Selection | Month grid with day selection |
| 3 | checklist | Selection | Multi-select list with checkboxes |
| 4 | color_picker | Selection | RGB channel slider with preview |
| 5 | context_menu | Selection | Popup list of actions |
| 6 | disk | Advanced | Disk/partition selector |
| 7 | file_picker | Selection | File system browser |
| 8 | filter | Selection | Searchable/filterable list |
| 9 | form | Input | Multi-field form with submit |
| 10 | gauge | Display | Percentage bar with label |
| 11 | hub | Container | Multi-category navigation hub |
| 12 | input | Input | Single-line text input |
| 13 | macro_recorder | Tool | Session macro record/playback |
| 14 | menu | Selection | Single-choice list |
| 15 | msg | Display | Message box with title and footer |
| 16 | multiselect | Selection | Multi-choice tag selector |
| 17 | notification | Display | Temporary toast notification |
| 18 | password | Input | Masked text input |
| 19 | progress | Display | Command output with progress |
| 20 | radio_group | Selection | Radio button group |
| 21 | range_slider | Input | Numeric range with slider bar |
| 22 | rich_text | Display | Markdown-formatted text |
| 23 | separator | Decoration | Horizontal or vertical line |
| 24 | spinner | Display | Animated spinner with message |
| 25 | split_panes | Container | Resizable split view (2-3 panes) |
| 26 | summary | Display | Summary/confirmation screen |
| 27 | table | Display | Row/column data grid |
| 28 | tabs | Container | Tabbed view with multiple children |
| 29 | terminal_emulator | Advanced | Embedded PTY terminal |
| 30 | text_editor | Input | Multi-line text editor |
| 31 | toggle | Input | On/off toggle switch |
| 32 | tooltip | Display | Hover/popup help text |
| 33 | tree | Display | Hierarchical tree view |
| 34 | widget_builder | Tool | Visual widget layout composer |
| 35 | yesno | Selection | Yes/No confirmation dialog |
| 36 | (reserved) | — | Reserved for future use |

---

## 6. Store & Reactive State

### 6.1 Subscriptions

Clients subscribe to store keys; daemon pushes `state` messages on change.

### 6.2 Reactive Expressions (FIL)

FIL validation scripts have store access via `store.keyname` syntax. The
`input` widget passes the active store to `fil_eval`.

---

## 7. Sessions

Sessions survive client disconnection via checkpoint/restore. Session
ownership tied to connecting user's UID. Animation state is maintained
per-session via `RenderTree.active_animations`.

---

## 8. Plugin System

### 8.1 Loading

`.so` files in `$HOME/.config/filly/plugins/`, loaded via `dlopen`/`dlsym`.

### 8.2 Signature Verification

Ed25519 detached signatures via libsodium. Tools provided.

### 8.3 Sandboxed Plugin Execution

Linux: seccomp. OpenBSD: pledge. FreeBSD: capsicum. Activated via `--sandbox`.

---

## 9. Themes & Style Engine

### 9.1 Theme Files

JSON theme files with inheritance via `extends`. 8 built-in themes plus
user overrides from `~/.config/filly/theme-override.json`.

### 9.2 Variables & Arithmetic

Variable resolution (`$name`) and colour arithmetic functions: `lighten`,
`darken`, `alpha`, `mix`.

### 9.3 Widget Style Selectors

`widget_type child_type:state` pattern with multi-layer cascade.

### 9.4 `WidgetStyle` Properties

Full property set including borders, padding, margins, shadows, gradients,
opacity, transitions, and animation transform properties (`scale_x`, `scale_y`,
`rotation`, `translate_x`, `translate_y`).

### 9.5 Transitions

Lerp-based property interpolation on state changes. Extended to all animatable
properties: colours, border dimensions, font size, opacity, shadows, gradients,
and animation transforms.

### 9.6 FIL-Extended Style Definitions

FIL `style` blocks applied via `theme_apply_fil_styles`.

### 9.7 Live Theme Reload

Protocol message triggers reload. inotify/kqueue file watching for automatic
reload on file changes.

### 9.8 Keybinding Maps

Default keymap (j/k/q/h/l/i). FIL `keymap` blocks parsed and loaded.

---

## 10. Animation Engine

### 10.1 Keyframe System

Multi-keyframe animations with per-segment easing functions. Supports:
`linear`, `ease-in`, `ease-out`, `ease-in-out`, `bounce`, `elastic`.

### 10.2 Animatable Properties

| Property | GPU | TUI |
|----------|-----|-----|
| fg_color, bg_color, border_color, accent_color | lerp | lerp |
| border_width, border_radius | int lerp | ignored |
| font_size, font_weight | int lerp | ignored |
| opacity | lerp | threshold dimming |
| shadow_offset, shadow_blur, shadow_color | lerp | ignored |
| bg_gradient_to | lerp | ignored |
| scale_x, scale_y, rotation | GPU transform | ignored |
| translate_x, translate_y | GPU transform | ignored |

### 10.3 Animation Definitions

Stored in theme JSON under `"animations"` key and in `.filly-project` files
from the builder. Named definitions registered in a global animation registry.

```json
{
  "animations": {
    "fadeIn": {
      "duration": 300,
      "keyframes": [
        {"time": 0, "easing": "ease-out", "opacity": 0.0},
        {"time": 300, "opacity": 1.0}
      ]
    }
  }
}
```

### 10.4 Animation Lifecycle

- `animation_play(tree, name, now_ms)` — starts a named animation
- `animation_stop(tree, name)` — stops and removes
- `animation_pause(tree, name)` / `animation_resume(tree, name, now_ms)` — pause/resume
- `animation_update(tree, now_ms)` — called per frame by session loop
- `animation_end` output port on every widget fires on completion
- `play_animation` input port accepts animation name strings

### 10.5 FIL Animation Statements

```
animate "widget_id" with "fadeIn"
stop animation on "widget_id"
pause animation on "widget_id"
resume animation on "widget_id"
```

### 10.6 Easing Functions

Implemented in `src/core/animation.c`:
`ease_linear`, `ease_in_quad`, `ease_out_quad`, `ease_in_out_quad`,
`ease_in_cubic`, `ease_out_cubic`, `ease_in_out_cubic`, `ease_bounce`,
`ease_elastic`.

### 10.7 TUI Animation Subset

Terminal backend applies opacity dimming, border colour cycling, and
character-level typewriter reveal. Transform properties (scale, rotation,
translation) are ignored in TUI mode.

---

## 11. GUI Builder

### 11.1 `filly-build` Binary

Standalone binary linking against FILLY core. Provides visual environment for
composing widget layouts, defining inter-widget connections, configuring
keyboard navigation, and generating self-contained plugin directories.

### 11.2 Application Shell

Five-pane layout using `split_panes` (with three-pane split):
Palette, Canvas, Property Editor, Connection Graph, and Status Bar.

### 11.3 Canvas

- Infinite zoom (0.1x to 5.0x), pan, snap-to-grid
- 8-point resize handles, rubber-band multi-select
- Right-click context menu (delete, lock, bring to front, send to back)
- Live pixel preview via headless backend
- Tab order overlay and grid toggle

### 11.4 Connection Graph Editor

- Node/port/edge visual diagram
- Drag wires from output ports to input ports
- Port compatibility checking (TRIGGER→any, type matching)
- Edge condition/transform editing (FIL expressions)
- Custom user-defined ports on any node
- Auto-node creation when canvas items added

### 11.5 Property Editor

- Dynamic form generation from widget `ParamDesc` arrays
- Keyboard editing with field navigation
- Live parameter update triggers preview re-render

### 11.6 Code Generation

- FIL script generation from connection graph edges
- Complete C plugin source with `build_layout()`, event handlers, factory
- Makefile output for standalone compilation
- Animation definitions embedded in generated code
- Headless CLI export mode (`--export` flag)

### 11.7 Validation Pipeline

13 automated checks: duplicate IDs, overlap, bounds, keyboard access,
tab order, colour contrast, FIL syntax, dangling edges, type mismatches,
cycles, missing labels, unused store vars, orphan widgets.

### 11.8 Keyboard Shortcuts

| Key | Action |
|-----|--------|
| F1-F4 | Mode switching (Edit/Wire/Preview/TUI) |
| Tab | Cycle panes (palette/canvas/properties) |
| Ctrl+S | Save project |
| Ctrl+E | Export plugin |
| +/- | Zoom in/out |
| g | Toggle grid |
| t | Toggle tab order overlay |
| a | Select all |
| v | Toggle validation panel |
| k | Enter keymap recording mode |

### 11.9 Project File Format

`.filly-project` JSON including items, nodes, edges, keymaps, store variables,
TUI configuration, and animation definitions.

---

## 12. Accessibility & Internationalisation

### 12.1 Accessibility Profiles

High-contrast and large-print themes. `set_accessibility` protocol message.

### 12.2 Internationalisation

gettext initialization. `_()` macro. `.pot` template generation. RTL detection.

---

## 13. Tooling & Correctness

### 13.1 Headless Test Suite

140+ behavioural tests covering all 36 widgets across terminal, headless
character, and headless pixel backends. All passing.

### 13.2 Cross-Backend Widget Tests

Every widget type is tested in **terminal**, **graphical (headless pixel)**, and
**headless character** modes. A single JSON request is rendered by all three
backends and compared for structural integrity, content fidelity, and style
resolution. Snapshot testing (ANSI and pixel) validates visual output.

### 13.3 Snapshot Testing

Pixel and ANSI snapshot comparison with `--generate` and `--mode` flags.
Reference snapshots stored in `test/snapshots/`.

### 13.4 Fuzzing

libFuzzer harness with CI integration.

### 13.5 Static Analysis

`.clang-tidy` config. Makefile targets: `lint`, `cppcheck`. Zero warnings
under `-std=c99 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -pedantic`.

### 13.6 Memory Safety

Arena allocator per frame. Valgrind CI. Zero leaks in test suite.

### 13.7 Fault Injection

Tests corrupted JSON, truncated messages, arena exhaustion, null parameters,
empty strings, massive choice lists.

### 13.8 Performance Benchmarks

Measures frame time, arena peak, response status for standard workloads.
Outputs CSV. Benchmarks run in CI with regression thresholds.

### 13.9 Macro Recording & Time-Travel Debugging

Records frames and events to `.filly-rec` JSON. Replay mode compares responses
against recorded snapshots. Macro recorder widget provides UI.

---

## 14. Client Libraries, Tools & Shell Wrappers

### 14.1 C Client Library

Full API: connect, send, send_key, poll, get_response, draw callback.

### 14.2 `filly send`

One-shot CLI for daemon communication.

### 14.3 `filly build`

JSON constructor with `--file` template substitution.

### 14.4 `filly relay`

Interactive TTY bridge. Properly restores terminal state on exit.

### 14.5 `filly oneshot`

Direct terminal or headless rendering. Supports `--gui`, `--headless`,
and `--tui` flags for explicit backend selection.

### 14.6 `filly compile`

Compiles widget JSON into binary `.filly` format.

### 14.7 `filly update`

Self-updating binary.

### 14.8 `filly-build`

GUI builder for visual widget composition and plugin generation.

### 14.9 Shell Wrappers

`fil.sh` for Bash integration.

### 14.10 Language Bindings

Python (ctypes), Go (cgo), Node.js (napi) linking against `libfilly.so`.

### 14.11 Native Installer Hub

`artixforge-hub` binary linked against client library.

---

## 15. The FIL Scripting Language

### 15.1–15.4 Grammar, Keywords, Data Model, Built-in Functions

Fully implemented recursive-descent parser and interpreter with animation
statements (`animate`, `stop`, `pause`, `resume`).

### 15.5 Integration

Wired to widget validation (with store access), style definitions,
animation control, and keybinding maps.

### 15.6 Sandbox

1-second timeout, 10,000 loop cap, no file I/O, no network, no shell.

### 15.7 Implementation

Single C file. Dependencies: JSON parsing, POSIX `regex.h`.

---

## 16. Security Model

### 16.1–16.4 Socket, Peer Cred, TTY, Plugin Signatures

Fully implemented with `filly-port/` cross-platform abstraction.

### 16.5 Plugin Sandboxing

Linux: seccomp. OpenBSD: pledge. FreeBSD: capsicum. `--sandbox` flag.

### 16.6 Checkpoint Sanitisation

Sensitive keys excluded from serialization.

---

## 17. Portability

FILLY is written in **strict ISO C99** with **POSIX.1-2008** as the only
required standard. No GNU extensions, no compiler-specific attributes, no
platform-specific functions in the core library. The build compiles cleanly
on GCC, Clang, musl-gcc, and slimcc.

### 17.1 Compiler Requirements

- `-std=c99 -D_POSIX_C_SOURCE=200809L`
- Zero warnings under `-Wall -Wextra -pedantic`
- No `_GNU_SOURCE`, `_DEFAULT_SOURCE`, or other feature-test macros
- No `__attribute__` extensions (replaced by `(void)` casts and portable
  alternatives)
- No nested functions
- No `strcasecmp` (replaced by internal `str_case_eq`)
- No VLAs in function parameters or struct members

### 17.2 Platform Support

| Feature | Linux | FreeBSD | OpenBSD |
|---------|-------|---------|---------|
| Peer credentials | `SO_PEERCRED` | `LOCAL_PEERCRED` | `getpeereid` |
| File watching | `inotify` | `kqueue` | `kqueue` |
| Sandboxing | `seccomp` | `capsicum` | `pledge` |
| Memory allocation | `malloc`/`realloc` | `malloc`/`realloc` | `malloc`/`realloc` |
| Shared memory | POSIX `shm_open` | POSIX `shm_open` | POSIX `shm_open` |
| Dynamic loading | `dlopen`/`dlsym` | `dlopen`/`dlsym` | `dlopen`/`dlsym` |
| Threads | POSIX pthreads | POSIX pthreads | POSIX pthreads |

All platform-specific code is isolated in `src/filly-port/` header files
(`port_linux.h`, `port_freebsd.h`, `port_openbsd.h`). The core library
and all backends compile on all three platforms, though graphical backends
require platform-specific display servers (Wayland/X11/DRM on Linux).

### 17.3 libc Compatibility

Tested and supported:
- glibc (Linux)
- musl (Linux, Alpine)
- FreeBSD libc
- OpenBSD libc

CI runs on all three platforms via native runners and vmactions.

---

## 18. Future-Proofing & Roadmap

### v1.1

- Animation timeline editor in `filly-build`
- GPU shader-based rendering for animations
- Container nesting via drag-and-drop in canvas
- Compile-and-load: test generated plugins directly
- Benchmark/fault-inject CI regression enforcement
- Widget snapshot gallery for visual regression testing

### v1.2+

- Gesture and eye tracking input
- Terminal emulator: scrollback search with regex
- Macro recording: edit and merge recordings
- Context-aware widget resolution with device profile cascading
- Distributed session sharing
- Voice input integration