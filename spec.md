**# FILLY Specification — v0.5 (Pre‑1.0)**

**FILLY** is a universal UI server for operating system deployment, configuration,
and any interactive terminal or graphical workflow.  It speaks a formal,
versioned protocol over Unix sockets, renders to terminals, graphical surfaces,
and in‑memory buffers, and is designed to be **provably correct**, **self‑healing**,
and **future‑proof**.  As of v0.5, FILLY ships its own C client library and
command‑line tools, eliminating external dependencies for all communication
paths.

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
10. [Accessibility & Internationalisation](#10-accessibility--internationalisation)
11. [Tooling & Correctness](#11-tooling--correctness)
12. [Client Libraries, Tools & Shell Wrappers](#12-client-libraries-tools--shell-wrappers)
13. [The FIL Scripting Language](#13-the-fil-scripting-language)
14. [Security Model](#14-security-model)
15. [Portability](#15-portability)
16. [Future‑Proofing & Roadmap](#16-future-proofing--roadmap)

---

## 1. Protocol Fundamentals

FILLY uses **newline‑delimited JSON (NDJSON)** as the default wire format.
A connection may optionally negotiate a binary encoding.

### 1.1 Transport

- **Unix socket** — primary transport, bound to `$XDG_RUNTIME_DIR/filly.sock`
  with `0600` permissions (falls back to `/tmp/filly.sock`).
- **Standard I/O** — oneshot mode reads a single request from `stdin` and
  writes the response to `stdout`.

### 1.2 Handshake & Encoding Negotiation

The very first message sent by a client **may** be a handshake.  If no handshake
is received, the daemon assumes NDJSON.

```json
{"type":"handshake","version":1,"encoding":"msgpack"}
```

The daemon replies with the same encoding and all subsequent messages use that
encoding.  Supported encodings: `json` (default), `msgpack` (negotiated, not
yet implemented as wire format).

### 1.3 Schema Validation

Every incoming message is validated against a strict JSON Schema before
dispatch.  Unknown fields are rejected; malformed messages receive a detailed
error response.  All string lengths, integer ranges, and required fields are
enforced.

### 1.4 Message Framing

All messages are single-line JSON objects terminated by `\n`.  Binary payloads
(such as draw data) are preceded by a JSON header containing a `len` field,
followed by exactly `len` raw bytes and a trailing newline.

```
{"type":"draw","len":4096}\n
<4096 bytes of raw data>\n
```

---

## 2. Message Catalog

Every message carries a `"type"` field.  Widget requests are backward‑compatible:
a message without a `"type"` but with a `"widget"` field is treated as a widget
request.

### 2.1 Lifecycle Messages

| type | Direction | Purpose |
|------|-----------|---------|
| `handshake` | Client→Daemon | Negotiate encoding |
| `ping` | Client→Daemon | Keep‑alive; daemon replies `pong` |
| `pong` | Daemon→Client | Response to `ping` |
| `quit` | Client→Daemon | Graceful shutdown |
| `session` | Client→Daemon | Create, attach, destroy a persistent session |
| `subscribe` | Client→Daemon | Subscribe to store key changes |
| `unsubscribe` | Client→Daemon | Remove a subscription |
| `state` | Daemon→Client | Push notification of a store change |
| `reload_theme` | Client→Daemon | Reload theme and style files at runtime |
| `reload_plugins` | Client→Daemon | Reload plugins (planned) |
| `set_accessibility` | Client→Daemon | Activate an accessibility profile |

### 2.2 Rendering Messages

| type | Direction | Purpose |
|------|-----------|---------|
| `draw` | Daemon→Client | Frame data follows (length‑prefixed) |
| `yield` | Daemon→Client | Intermediate progress update |
| `response` | Daemon→Client | Final result of a widget request |

**`draw`** is a length‑prefixed binary blob.  For terminal backends it contains
ANSI escape sequences; for graphical backends it contains raw pixel data or
rendering commands:

```json
{"type":"draw","len":4096}
<4096 bytes of ANSI or pixel data>
```

**`yield`** carries arbitrary progress data:

```json
{"type":"yield","progress":50,"stage":"Compiling"}
```

**`response`** carries the final widget result:

```json
{"type":"response","result":"XFCE","cancelled":false}
```

### 2.3 Widget Requests (Backward Compatible)

No `"type"` field; presence of `"widget"` identifies it.

```json
{
  "widget":"menu",
  "params":{"title":"Desktop","choices":["KDE","XFCE"]},
  "relay":true,
  "session_id":"abc123",
  "headless":false,
  "gui":false
}
```

When `"relay":true`, the daemon expects a relay client to handle TTY I/O
and forward keystrokes as `{"type":"key","code":…,"ch":"…"}` messages over
the same connection.  If `"headless":true` is present, the widget renders
into an in‑memory buffer.  If `"gui":true`, the graphical backend is used.

### 2.4 Input Messages

| type | Direction | Purpose |
|------|-----------|---------|
| `key` | Client→Daemon | Keystroke injection (`code` and `ch` fields) |
| `mouse` | Client→Daemon | Mouse event injection (`x`, `y`, `button`, `state`) |
| `resize` | Client→Daemon | Terminal / window resize (`w`, `h`) |

Mouse events carry additional fields:
```json
{"type":"mouse","x":120,"y":45,"button":1,"state":"press"}
```
States: `motion`, `press`, `release`, `scroll`.

---

## 3. Daemon Lifecycle & Resilience

### 3.1 Startup

- Loads plugins from `$HOME/.config/filly/plugins/`.
- Loads style files from `$HOME/.config/filly/styles/` and any plugin style
  overrides.
- Binds to Unix socket at `$XDG_RUNTIME_DIR/filly.sock` (`0600`), falling back
  to `/tmp/filly.sock`.
- Redirects stdin/stdout/stderr to `/dev/null` after binding.
- Validates plugin Ed25519 signatures against an embedded public key; unsigned
  plugins are rejected unless `--insecure-plugins` is set.
- Restores any checkpointed sessions from the previous run.
- Enters accept loop.

### 3.2 Connection Handling

- Each connection is a separate thread.
- **SO_PEERCRED verification:** on Linux and systems supporting `SO_PEERCRED`,
  the daemon validates that the connecting process's UID matches its own,
  rejecting connections from other users with a permission‑denied error.
  Falls back to socket permissions only on platforms without peer credential
  support.
- Inactivity timeout: clients that do not send any message within 30 seconds
  are disconnected with an error response; their sessions are preserved but
  marked inactive.
- Session isolation: sessions are scoped to the connecting user; a client
  cannot attach to another user's session.

### 3.3 Crash Resilience

The daemon periodically writes a **checkpoint** of all active sessions (store
state) to `~/.cache/filly/checkpoint.json` (`0600`).  Sensitive keys (pass,
LUKS, token, key, secret) are excluded from serialization.  On crash, a new
daemon instance loads the checkpoint and resumes listening.  Clients reconnect
and attach to their existing sessions.

---

## 4. Backends

FILLY's `BackendVTable` abstraction allows the same widget code to render to
any output surface.  Backends are selected at runtime via the `--gui` flag
or automatic detection.

### 4.1 Terminal Backend

- Raw ANSI escape sequences, 24‑bit true colour with 256‑colour fallback.
- Synchronised output (`\033[?2026h`) to prevent tearing.
- Incremental rendering: only changed regions are redrawn.
- Mouse support (SGR extended coordinates) — parsed and dispatched to widgets
  as mouse events.
- Clipboard integration via OSC 52 (`Ctrl+C`/`Ctrl+V`).

### 4.2 `filly-gcore` — Native Graphical Backend (Planned v0.6)

A unified pixel‑based renderer that translates the abstract `RenderTree` into
pixels and displays them through swappable output targets.  It replaces the
deprecated Python GTK4 backend.

**Pixel Renderer:**
- Zero‑dependency text rendering via `stb_truetype` (single‑header, public domain).
- Glyph cache pre‑rasterised at startup for ASCII printable characters.
- Font stacks with fallback: `"Inter", "Noto Sans", "sans-serif"`.
- Layout engine: vertical stacks, horizontal rows, weighted distribution.
- Full `WidgetStyle` support (colours, borders, shadows, gradients, opacity,
  rounded corners, padding, margins, transitions).
- Frame‑damage tracking and partial redraws for energy efficiency.
- GPU acceleration via EGL/OpenGL ES where available; software rasterisation
  fallback.

**Output Targets (swappable at runtime):**

| Target | Mechanism | Use Case |
|--------|-----------|----------|
| DRM/KMS | Direct page flip via `libdrm` | Full‑screen TTY (installer, kiosk) |
| X11 | `XPutImage` or SHM pixmap | Desktop alongside other windows |
| Wayland | Shared‑memory `wl_buffer` | Modern desktop compositors |
| Headless Pixel | Off‑screen pixel buffer | Testing, screenshots, CI snapshot diffs |

The DRM backend opens a connected display connector, sets the native mode,
allocates dumb buffers (or GBM buffers for GPU), and performs double‑buffered
page flips.  Input is read via `libinput` and translated to FILLY `Event`
structs.

The X11 and Wayland backends create a single window and handle input via
their native event queues.

**Graceful Degradation:**
At startup, the daemon probes backends in priority order:
DRM → X11 → Wayland → terminal TUI.
The first successfully initialised backend is used.  Applications need not
care which backend is active.

**Mouse‑to‑Key Synthesis:**
For immediate compatibility with all existing widgets, the graphical backends
perform hit‑testing on the `RenderTree` and synthesise keyboard events from
mouse clicks.  Clicking a list item generates `KEY_ENTER`; clicking elsewhere
generates `KEY_ESC`.  This works for all 33+ widgets without modification.
Native mouse handling can be added per‑widget incrementally.

### 4.3 Headless Backend

- Renders to an in‑memory character buffer (terminal mode) or pixel buffer
  (graphical mode).
- Accepts synthetic key and mouse events via `KEY:`, `TEXT:`, `WAIT:` directives
  (and `MOUSE:` for GUI tests).
- Returns final JSON response.
- Auto‑EOF sentinel: when the injected event queue drains, a single ESC is
  synthesised; session exits after 5000 idle cycles.
- Used for CI testing, 119‑test behavioural suite, snapshot testing, and replay
  debugging.

### 4.4 Multi‑Surface Rendering

A single daemon session can render simultaneously to:
- A TTY
- A headless buffer
- A graphical surface (one per display)
Reserved: PNG screenshot stream, WebSocket stream.

---

## 5. Widget System

All existing 33 widget types are preserved with identical parameters.

### 5.1 Widget Composition

Widgets may contain child widgets defined recursively in JSON:

```json
{
  "widget":"tabs",
  "params":{
    "tabs":[
      {"label":"General","widget":{"widget":"input","params":{...}}},
      {"label":"Advanced","widget":{"widget":"checklist","params":{...}}}
    ]
  }
}
```

The parent widget creates and manages children through the existing vtable.

### 5.2 Embedded FIL Scripting

Widgets and plugins may carry small FIL scripts for validation, filtering,
dynamic content, and **style overrides** (see §9 and §13).

```json
{
  "widget":"input",
  "params":{
    "title":"Hostname",
    "validation_script":"when length of value is less than 3 then reject with \"Too short\" end\naccept"
  }
}
```

### 5.3 Widget Lifecycle & State Machine

Each widget transitions through a formal state machine:

```
[created] → (first render) → [active] → (response sent) → [completed]
                                  ↓
                              [suspended] → (resume) → [active]
                                  ↓
                              [destroyed]
```

`handle_event` returns `EVENT_RESULT_RESPONSE` to complete,
`EVENT_RESULT_SUSPEND` to yield control (e.g., sub‑widget takes over),
`EVENT_RESULT_HANDLED` to continue.  The session loop manages the stack
of active/suspended widgets.

### 5.4 Undo/Redo Stack

The session layer provides a generic action stack for reversible operations.
Widgets push actions with `undo`/`redo` callbacks.  `Ctrl+Z` / `Ctrl+Shift+Z`
pop from the stack.  Beneficial for text editors, form fields, disk partitioners,
and user managers.

---

## 6. Store & Reactive State

Every session has a key‑value store.  Widgets may **bind** to store keys.
When a key changes, bound widgets are automatically re‑rendered — no manual
dirty flags.

### 6.1 Subscriptions

Clients can subscribe to store keys:

```json
{"type":"subscribe","keys":["DISK","FS_TYPE"]}
```

The daemon pushes changes:

```json
{"type":"state","key":"DISK","value":"/dev/sda"}
```

### 6.2 Reactive Expressions (FIL)

Store bindings may include FIL expressions for derived values:

```
bind ROOT_PART to derive_root_part of DISK
```

---

## 7. Sessions

### 7.1 Persistent Sessions

A session survives client disconnection.  State is checkpointed and restored
on reconnection.

```json
{"type":"session","action":"create"}
→ {"type":"session","id":"abc123","action":"created"}

{"widget":"menu","params":{...},"session_id":"abc123"}
```

Session ownership is tied to the user who created it (via `SO_PEERCRED` UID).

---

## 8. Plugin System

### 8.1 Loading

- Plugins are `.so` files in `$HOME/.config/filly/plugins/`.
- Each exports `register_plugins(void (*reg)(const char*, WidgetFactory))`.
- Loaded at daemon startup and in oneshot mode.

### 8.2 Signature Verification

Every `.so` must have a corresponding `.so.sig` — a 64‑byte Ed25519 detached
signature produced by the plugin author.  The daemon verifies the signature
against a bundled public key using libsodium `crypto_sign_verify_detached`
before `dlopen`.  Unsigned plugins are rejected unless `--insecure-plugins`
is explicitly passed.  Keypair generation and signing are provided by
`tools/genkey` and `tools/sign`.

### 8.3 Sandboxed Plugin Execution (Planned v0.7)

Plugin widget factories and event handlers run in a forked child process with
`seccomp` (Linux) or `pledge` (OpenBSD) restrictions.  Only the resulting
`RenderTree` is passed back over a pipe.  Resource limits (`RLIMIT_AS`,
`RLIMIT_CPU`) prevent runaway plugins from exhausting system memory or
saturating the CPU.  This prevents malicious or buggy plugins from crashing
the daemon or reading sensitive store keys.

---

## 9. Themes & Style Engine

### 9.1 Theme Files

Themes are JSON files in `themes/`.  They support **inheritance** via `extends`:

```json
{
  "name":"custom",
  "extends":"nord",
  "variables": {
    "accent": "#ff0000",
    "surface": "#1a1a2e"
  },
  "global": {
    "font_family": "Inter",
    "font_size": 14,
    "fg_color": "$text-primary",
    "bg_color": "$surface",
    "border_radius": 8,
    "transition_ms": 150
  },
  "widgets": {
    "button": {
      "bg_color": "$accent",
      "fg_color": "#ffffff",
      "border_radius": 6
    },
    "button:hover": {
      "opacity": 0.85
    },
    "input:focus": {
      "border_color": "$accent",
      "shadow_color": "$accent",
      "shadow_blur": 4
    }
  }
}
```

### 9.2 Variables & Arithmetic

Variables declared in the `variables` block are referenced via `$name` and
resolved recursively.  Built‑in colour functions: `lighten(color, amount)`,
`darken(color, amount)`, `alpha(color, opacity)`, `mix(color1, color2, weight)`.

### 9.3 Widget Style Selectors

Selectors follow the pattern `widget_type [child_type] [:state]`:

| Example | Meaning |
|---------|---------|
| `menu` | Applies to the menu widget |
| `menu item` | Applies to list items inside a menu |
| `hub sidebar` | Applies to the sidebar panel of the hub |
| `button:hover` | Applies to buttons under the mouse cursor |
| `button:pressed` | Applies to buttons being clicked |
| `input:focus` | Applies to the active input field |
| `input:invalid` | Applies to an input field failing validation |
| `checkbox:checked` | Applies to checked checkboxes |

The style engine resolves the cascade: base theme → plugin overrides →
application overrides → user overrides → accessibility profile.

### 9.4 `WidgetStyle` Properties

The resolved style for each render node is a struct containing:

| Property | Type | Description |
|----------|------|-------------|
| `fg_color`, `bg_color`, `border_color`, `accent_color` | RGBA | Colours |
| `border_width` | int (0‑n) | Border thickness in pixels |
| `border_radius` | int | Corner radius |
| `padding` | int[4] | Top, right, bottom, left |
| `margin` | int[4] | Outer spacing |
| `min_width`, `min_height`, `max_width`, `max_height` | int | Size constraints |
| `font_family` | string | Font stack, e.g., `"Inter", "sans"` |
| `font_size` | int | Points |
| `font_weight` | int | 100‑900 |
| `font_italic` | bool | |
| `text_align` | enum | left, center, right |
| `opacity` | float | 0.0–1.0 |
| `shadow_offset_x`, `shadow_offset_y`, `shadow_blur`, `shadow_color` | int,int,int,RGBA | Box shadow |
| `bg_gradient`, `bg_gradient_to`, `bg_gradient_direction` | bool, RGBA, int | Background gradient |
| `transition_ms` | int | Duration of property transitions |

### 9.5 Transitions & Animations

When a state change occurs (e.g., `normal` → `hover`), the renderer interpolates
numeric and colour properties over `transition_ms`.  Supported easing curves:
`linear`, `ease-in`, `ease-out`, `ease-in-out`.

Pre‑defined animations (fade, slide, scale) can be attached to widget lifecycle
events:

```json
"animations": {
  "page_enter": { "type": "slide", "direction": "left", "duration_ms": 200 },
  "dialog_open": { "type": "fade-scale", "duration_ms": 150 }
}
```

### 9.6 FIL‑Extended Style Definitions (Planned v0.7)

Style definitions can be written in FIL with dedicated `style` blocks, enabling
programmatic control:

```
let accent to #e94560
style hub {
    bg is darken of accent with 10
    border-radius is 12
}
style menu item:hover {
    transition is 200ms
    bg is lighten of accent with 5
}
```

This reuses the existing FIL parser and interpreter, adding `style` and
`animate` statement types.  Styles compile to the same binary style database
used by the pixel renderer.

### 9.7 Live Theme Reload

A control message `{"type":"reload_theme"}` causes the daemon to reload all
theme and style files from disk and re‑render all active widgets.  On platforms
supporting file watching (`inotify` / `kqueue`), the daemon can watch the
theme directories and reload automatically.

### 9.8 Keybinding Maps

Input bindings are part of the style layer.  A keymap file associates keyboard
and mouse gestures with actions:

```
keymap global {
    Escape is back
    Ctrl-Q is quit
}
keymap menu {
    j is move-down
    k is move-up
    Enter is select
    Ctrl-A is select-all
}
```

Gestures for touch surfaces and gamepads are defined similarly.  The keymap
is resolved per‑widget and per‑state by the style engine.

---

## 10. Accessibility & Internationalisation

### 10.1 Accessibility Metadata

Every render node carries an `accessible` object:

```json
{"type":"text","content":"Hostname","accessible":{"role":"label","label":"Hostname input"}}
```

**Accessibility Profiles** are style overrides that activate when an
accessibility profile is loaded:

```json
"accessibility": {
  "high-contrast": {
    "global": {
      "fg_color": "#ffffff",
      "bg_color": "#000000",
      "border_color": "#ffff00",
      "font_size": 1.5,
      "shadow": "none"
    }
  },
  "screen-reader": {
    "global": {
      "accessible-role": "$widget-id",
      "accessible-label": "$widget-label"
    }
  }
}
```

A screen‑reader backend walks the render tree and speaks via Speech Dispatcher
(planned).

### 10.2 Keyboard Navigation

All widgets are fully operable by keyboard.  Tab order, focus indicators, and
accelerator keys are defined in the render tree and customisable via keymaps.

### 10.3 Internationalisation

All user‑visible strings are translatable via POSIX `gettext`.  The daemon
accepts a `lang` parameter.  RTL text rendering (Arabic, Hebrew) is handled
by the layout engine: alignment mirrors and character order reverses when a
RTL locale is active.  Font stacks include CJK fallback.

### 10.4 High‑Contrast & Large‑Print

Built‑in high‑contrast and large‑font accessibility profiles are provided.

---

## 11. Tooling & Correctness

### 11.1 Headless Test Suite

Every widget has a headless test: render initial state, inject key events via
`KEY:`, `TEXT:`, and `WAIT:` directives, assert final response matches expected
values.  119 behavioural tests cover all 33 built‑in widgets plus 9 ArtixForge
and 6 GForge plugin widgets.  A native C test binary (`filly-test`) also
exercises core widgets and the client library programmatically.

### 11.2 Snapshot Testing

ANSI output (terminal) and pixel output (headless GUI) of every widget in every
theme is captured as a reference file.  CI diffs against the reference.  Any
rendering change is intentional.

### 11.3 Fuzzing

The protocol parser is fuzzed with `libFuzzer` (planned).

### 11.4 Static Analysis

Zero warnings with `gcc -std=c99 -Wall -Wextra -O2`.  `clang-tidy`, `cppcheck`,
and MISRA C:2012 compliance planned for v0.6.

### 11.5 Memory Safety

- All external input bounds‑checked via cJSON and schema validation.
- **Arena allocator per frame** — a bump‑pointer allocator reset at the end of
  each render pass eliminates per‑node `malloc`/`free`.  Reduces heap pressure
  and simplifies memory management.
- Valgrind/ASan clean enforced in CI (planned).

### 11.6 Profiling Overlay

A control message `{"type":"toggle_perf_overlay"}` enables an in‑GUI heads‑up
display showing FPS, frame render time (total and per‑widget), and arena
memory usage.  Compiles out entirely with `-DFILLY_PROFILING=OFF`.

---

## 12. Client Libraries, Tools & Shell Wrappers

### 12.1 C Client Library (`filly-core/client.h`)

The canonical client implementation.  Handles Unix socket connection, NDJSON
framing, draw‑frame reassembly, key/mouse injection, and response extraction.
All other clients are thin wrappers around this library.

**API:**
```c
FillyClient *filly_client_connect(const char *socket_path);
void         filly_client_disconnect(FillyClient *c);
int          filly_client_send_request(FillyClient *c, const char *json);
int          filly_client_send_key(FillyClient *c, int keycode, char ch);
int          filly_client_send_quit(FillyClient *c);
int          filly_client_poll(FillyClient *c, int timeout_ms);
int          filly_client_get_response(FillyClient *c, cJSON **result, bool *cancelled);
void         filly_client_set_draw_callback(FillyClient *c,
                void (*cb)(const char *data, int len, void *user), void *user);
int          filly_client_get_fd(FillyClient *c);
int          filly_client_process(FillyClient *c);
bool         filly_client_has_response(FillyClient *c);
```

Non‑blocking I/O via `O_NONBLOCK` allows integration with external event loops
(`select`, `poll`, `epoll`).  `filly_client_process` reads available data and
processes it; `filly_client_has_response` indicates completion.

### 12.2 `filly send` — Universal One‑Shot CLI

Sends any NDJSON request to the daemon and prints the response result to stdout.
Replaces `nc`, `grep`, and `jq` for non‑interactive communication.

```bash
filly send '{"widget":"menu","params":{"title":"Pick","choices":["A","B"]}}'
filly send --subscribe DISK,FS_TYPE
filly send --quit
filly send --json '{"widget":"input",...}'
```

Exit code 0 on success, 1 on cancel/error.

### 12.3 `filly build` — JSON Constructor

Constructs NDJSON payloads from command‑line arguments.

```bash
filly build menu --title "Pick" --choice A --choice B --choice C | filly send
filly build yesno --title "Proceed?" --default yes | filly send
filly build input --title "Name" --placeholder "Enter name" | filly send
```

Supports `--file` for JSON templates with `${var}`/`$var` substitution via
`--set key=value`.

### 12.4 `filly relay` — Interactive Session Bridge

Connects to the daemon, puts the terminal in raw mode, forwards draw frames to
the TTY, and forwards keystrokes.  Built on the C client library.

```bash
filly relay /tmp/filly.sock '{"widget":"install_hub","params":{...},"relay":true}'
```

Performs TTY ownership validation before opening `/dev/tty`.

### 12.5 Deprecated Shell Wrappers

The legacy `fil.sh` and `filly_graphical.sh` shell wrappers are deprecated in
v0.5 and will be removed in v0.6.  Migrate to `filly send` and `filly build`.

### 12.6 Language Bindings

Thin wrappers over the C client library:

- **Python** — ctypes binding to `filly-core/client.h`.  Provides
  `filly.Client` context manager.
- **Go** — cgo binding.  Provides `go-filly` package with connection pooling.
- **Node.js** — napi binding.  Provides `node-filly` npm package with
  async/await.

### 12.7 Native Installer Hub (`artixforge-hub`)

The ArtixForge installer ships a dedicated C binary in
`plugins/artixforge/artixforge-hub.c` that:
- Reads installer state from `/tmp/artix-installer/state.conf`
- Discovers disks via `lsblk`
- Reads data files (`kernels.txt`, `timezones.txt`, etc.)
- Constructs the full hub JSON using `cJSON`
- Calls `filly_client_connect` and runs the relay loop natively

The installer's `tui_afhub` function becomes:
```bash
"${BASE_DIR}/artixforge-hub" --state "${STATE_FILE}" \
    --data-dir /tmp/artix-installer/filly-data
```

---

## 13. The FIL Scripting Language

**FIL** (FILLY Interpreted Language) is a tiny, English‑like scripting language
embedded in the daemon.  It is used for widget validation, conditional
visibility, state transformations, **style definitions**, and any user‑defined
logic that does not require a full programming language.

**Design goals:** zero dependencies, POSIX‑compliant C implementation,
sandboxed execution, human‑readable syntax.

### 13.1 Grammar

```
script         = statement*
statement      = conditional | assignment | action | loop | style_block | animate | keymap
conditional    = "when" expression "then" statement* ("else" statement*)? "end"
assignment     = ("let"|"set") identifier "to" expression
action         = "reject" "with" string | "accept" | "show" "when" expression
loop           = "for" "each" identifier "in" identifier "do" statement* "end"
style_block    = "style" identifier (" " identifier)* "{" style_property* "}"
style_property = identifier "is" value ("with" value)*
animate        = "animate" identifier "from" identifier "over" number "ms" ("easing" identifier)?
keymap         = "keymap" identifier "{" keymap_entry* "}"
keymap_entry   = "when" "key" "is" identifier "then" identifier "end"
expression     = comparison (("and"|"or") comparison)*
comparison     = value (("is"|"is not"|"matches"|"is less than"|"is greater than") value)?
value          = "not"? primary
primary        = identifier | string | number | "empty" | function_call | "(" expression ")"
function_call  = identifier "of" value ("with" value)?
identifier     = [a-zA-Z_][a-zA-Z0-9_-]*
string         = '"' [^"]* '"' | "#" [0-9a-fA-F]+
number         = [0-9]+ ("." [0-9]+)? ("ms")?
```

### 13.2 Keywords

`when`, `then`, `else`, `end`, `let`, `set`, `to`, `reject`, `with`, `accept`,
`show`, `for`, `each`, `in`, `do`, `is`, `not`, `matches`, `and`, `or`,
`empty`, `of`, `less`, `greater`, `than`, `length`, `uppercase`, `lowercase`,
`trim`, `number`, `match`, `style`, `animate`, `keymap`, `easing`, `ms`.

### 13.3 Data Model

All values are strings.  Boolean operations treat any non‑empty string as true.
String comparisons are lexicographic.  Integer arithmetic is performed by
converting to `int` internally.  Colour literals are hex strings (`#e94560`);
the style engine converts them to RGBA.

### 13.4 Built‑in Functions

| Function | Description |
|----------|-------------|
| `length of STRING` | Number of characters |
| `uppercase of STRING` | Convert to upper case |
| `lowercase of STRING` | Convert to lower case |
| `trim of STRING` | Remove leading/trailing whitespace |
| `number of STRING` | Number of elements in a JSON array string |
| `match of STRING with PATTERN` | POSIX extended regex match |
| `lighten of COLOR with AMOUNT` | Increase colour lightness |
| `darken of COLOR with AMOUNT` | Decrease colour lightness |
| `alpha of COLOR with OPACITY` | Set alpha channel |
| `mix of COLOR with COLOR with WEIGHT` | Blend two colours |

### 13.5 Integration

FIL scripts are embedded in widget parameters, style files, and keymap files.
The daemon exposes the following variables to every script:

| Variable | Value |
|----------|-------|
| `value` | The current widget value |
| `store.KEY` | Any session store key |
| `display-width`, `display-height` | Current surface dimensions |

### 13.6 Sandbox

- No file I/O functions.
- No network functions.
- No shell execution.
- Loop iterations capped at 10 000.
- Execution time limit of 1 second via `SIGALRM`.
- All memory freed on script exit.

### 13.7 Implementation

A single C file (`fil.c`, `fil.h`) implementing a recursive‑descent parser,
AST, and interpreter.  Zero dependencies beyond `cJSON` (vendored) and POSIX
`regex.h`.

---

## 14. Security Model

### 14.1 Socket Permissions

The Unix socket is created with `0600` permissions in `$XDG_RUNTIME_DIR`
(`0700` by default).  Only the owning user can connect.

### 14.2 Peer Credential Verification

On platforms supporting `SO_PEERCRED` (Linux) or `LOCAL_PEERCRED` (FreeBSD),
the daemon validates that connecting clients share its UID.  Connections from
other users are rejected with a permission‑denied error.  On platforms without
peer credential support, the daemon relies on socket permissions alone.

### 14.3 TTY Ownership

All paths that open `/dev/tty` (`relay_run`, `artixforge-hub`, daemon relay
threads) verify that the target TTY is owned by the current user before opening
it, preventing cross‑user TTY hijacking.

### 14.4 Plugin Signature Verification

Ed25519 detached signatures via libsodium verify that plugins originate from
a trusted source.  Unsigned plugins are rejected unless `--insecure-plugins`
is explicitly passed.

### 14.5 Plugin Sandboxing (Planned v0.7)

Plugin widget code runs in a forked, sandboxed process with resource limits and
system call filtering.  Only the resulting `RenderTree` is passed back.

### 14.6 Checkpoint Sanitisation

Sensitive keys (pass, LUKS, token, key, secret) are excluded from checkpoint
serialization.

---

## 15. Portability

FILLY targets POSIX.1‑2008 compliance.  The core codebase compiles with
`gcc -std=c99 -D_POSIX_C_SOURCE=200809L` and uses only standard POSIX APIs.

Platform‑specific features are isolated in `filly-port/` and selected at
build time:

| Feature | Linux | FreeBSD | OpenBSD |
|---------|-------|---------|---------|
| Terminal I/O | `termios` | `termios` | `termios` |
| Unix sockets | `AF_UNIX` | `AF_UNIX` | `AF_UNIX` |
| Peer credentials | `SO_PEERCRED` | `LOCAL_PEERCRED` | `getpeereid` |
| Display | DRM/KMS | DRM/KMS | — |
| File watching | `inotify` | `kqueue` | `kqueue` |
| Sandboxing | `seccomp` | `capsicum` | `pledge` |

Optional backends (DRM, X11, Wayland) are compile‑time features.  The base
`filly` binary with terminal and headless backends builds on any POSIX system
with a C compiler and `libsodium`.

---

## 16. Future‑Proofing & Roadmap

### v0.6 — `filly-gcore` Native Graphical Backend

- Unified pixel renderer with `stb_truetype` text and layout engine.
- DRM/KMS output target (full‑screen).
- X11 and Wayland output targets (windowed).
- Headless pixel output for snapshot testing.
- Mouse event support in all backends; mouse‑to‑key synthesis for legacy widgets.
- Frame‑damage tracking and partial redraws.
- Graceful degradation across display backends.
- Extended `WidgetStyle` support (colours, borders, shadows, gradients).
- FIL‑based style definitions and keymap files.
- Arena allocator per frame.
- Removes Python GTK4 dependency.

### v0.7 — Advanced Features

- **Style engine cascade** — full inheritance, variables, arithmetic, colour
  functions, state selectors, transitions, animations.
- **Keybinding maps** — per‑widget, per‑state input bindings via FIL.
- **Sandboxed plugins** — `fork` + platform sandbox around plugin execution.
- **Hot‑reload plugins** — `dlclose`/`dlopen` without daemon restart.
- **Clipboard interface** — vtable‑based, per‑backend (OSC 52, X11 selections,
  Wayland data device, internal daemon clipboard).
- **Widget diffing** — send only changed screen regions during re‑render.
- **Declarative UI compilation** — JSON/FIL definitions compiled to widget
  trees at build time, producing `.filly` bundles.
- **Snapshot testing** — pixel output diffs in CI.
- **Fuzzing** — `libFuzzer` against protocol parser.
- **Memory‑mapped IPC** — high‑throughput shared‑memory streaming for GUI
  clients.
- **A11y profiles** — high‑contrast, large‑print, screen‑reader (via Speech
  Dispatcher).
- **Internationalisation** — `gettext` integration, RTL text rendering, CJK
  font fallback.
- **Undo/redo** — session‑level action stack for reversible widget operations.
- **Profiling overlay** — in‑GUI FPS, frame time, memory usage display.
- **Repository protocol** — HTTP API for discovering and installing plugins.
- **Self‑updating binary** — hot‑reload via `execve` without dropping clients.
- **Multi‑display** — one daemon, multiple independent graphical surfaces.

### v0.8+ — Long‑Term

- **Alternative input methods** — eye‑tracking, voice, gamepad plugins.
- **Built‑in terminal emulator widget** — PTY management + ANSI rendering
  within the GUI.
- **Macro recording & time‑travel debugging** — record/replay session events.
- **Context‑aware widget resolution** — `"widget":"auto"` selects best plugin
  widget for a context.
- **Fault injection** — simulate dropped bytes, malloc failures, slow clients.
- **Performance benchmarks** — keystroke‑to‑screen latency regression tests.