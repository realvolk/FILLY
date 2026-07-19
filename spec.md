# FILLY Specification — v0.4 (Pre‑1.0)

**FILLY** is a universal UI server for operating system deployment, configuration,
and any interactive terminal or graphical workflow.  It speaks a formal,
versioned protocol over Unix sockets, renders to terminals, graphical surfaces,
and in‑memory buffers, and is designed to be **provably correct**, **self‑healing**,
and **future‑proof**.

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
9. [Themes](#9-themes)
10. [Accessibility & Internationalisation](#10-accessibility--internationalisation)
11. [Tooling & Correctness](#11-tooling--correctness)
12. [Client Libraries & Shell Wrappers](#12-client-libraries--shell-wrappers)
13. [The FIL Scripting Language](#13-the-fil-scripting-language)
14. [Future‑Proofing & Roadmap](#14-future-proofing--roadmap)

---

## 1. Protocol Fundamentals

FILLY uses **newline‑delimited JSON (NDJSON)** as the default wire format.
A connection may optionally negotiate a binary encoding.

### 1.1 Transport

- **Unix socket** — primary transport, bound to `$XDG_RUNTIME_DIR/filly.sock`
  with `0600` permissions (falls back to `/tmp/filly.sock`).
- **TCP + TLS** — reserved for future remote access.
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

### 2.2 Rendering Messages

| type | Direction | Purpose |
|------|-----------|---------|
| `draw` | Daemon→Client | Frame data follows (length‑prefixed) |
| `yield` | Daemon→Client | Intermediate progress update |
| `response` | Daemon→Client | Final result of a widget request |

**`draw`** is a length‑prefixed binary blob:

```json
{"type":"draw","len":4096}
<4096 bytes of ANSI or graphical data>
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
  "headless":false
}
```

When `"relay":true`, the daemon opens `/dev/tty` directly for input and
output — no external relay process is involved.  If `"headless":true` is
present, the widget renders into an in‑memory buffer.

---

## 3. Daemon Lifecycle & Resilience

### 3.1 Startup

- Loads plugins from `$HOME/.config/filly/plugins/`.
- Binds to Unix socket at `$XDG_RUNTIME_DIR/filly.sock` (`0600`), falling back
  to `/tmp/filly.sock`.
- Redirects stdin/stdout/stderr to `/dev/null` after binding.
- Validates plugin Ed25519 signatures against an embedded public key; unsigned
  plugins are rejected unless `--insecure-plugins` is set.
- Restores any checkpointed sessions from the previous run.
- Enters accept loop.

### 3.2 Connection Handling

- Each connection is a separate thread.
- Inactivity timeout: clients that do not send any message within 30 seconds
  are disconnected with an error response; their sessions are preserved but
  marked inactive.

### 3.3 Crash Resilience

The daemon periodically writes a **checkpoint** of all active sessions (store
state) to `~/.cache/filly/checkpoint.json` (`0600`).  Sensitive keys (pass,
LUKS, token, key, secret) are excluded from serialization.  On crash, a new
daemon instance loads the checkpoint and resumes listening.  Clients reconnect
and attach to their existing sessions.

---

## 4. Backends

### 4.1 Terminal Backend

- Raw ANSI escape sequences, 24‑bit true colour with 256‑colour fallback.
- Synchronised output (`\033[?2026h`) to prevent tearing.
- Unicode grapheme cluster support via `libicu` (planned).
- Right‑to‑left text rendering (planned).
- Incremental rendering: only changed regions are redrawn.
- Mouse support (SGR extended coordinates) — parsed but not yet dispatched to
  widgets.
- Clipboard integration via OSC 52 (`Ctrl+C`/`Ctrl+V`) — parsed but not yet
  dispatched.
- Frame buffer access (DRM/KMS) — reserved for `filly-gcore` (v0.5).

### 4.2 Graphical Backend (Python, GTK4)

- GTK4 + libadwaita.
- Same JSON protocol as terminal backend.
- AT‑SPI accessibility bridge via Gtk accessibility metadata.
- Deprecated in favour of planned native `filly-gcore` renderer (v0.5).

### 4.3 Headless Backend

- Renders to an in‑memory character buffer.
- Accepts synthetic key events via `KEY:`, `TEXT:`, and `WAIT:` directives.
- Returns final JSON response.
- Auto‑EOF sentinel: when the injected event queue drains, a single ESC is
  synthesized; session exits after 5000 idle cycles.
- Used for CI testing, 119‑test behavioural suite, and replay debugging.

### 4.4 Multi‑Surface Rendering

A single daemon session can render simultaneously to:
- A TTY
- A headless buffer
Reserved for future: WebSocket stream, PNG screenshot stream.

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

Widgets and plugins may carry small FIL scripts for validation, filtering, or
dynamic content.  The daemon includes a built‑in FIL interpreter (see §13).

```json
{
  "widget":"input",
  "params":{
    "title":"Hostname",
    "validation_script":"when length of value is less than 3 then reject with \"Too short\" end\naccept"
  }
}
```

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

---

## 9. Themes

Themes are JSON files in `themes/`.  They support **inheritance**:

```json
{
  "name":"custom",
  "extends":"nord",
  "overrides":{
    "colors":{"accent":"#ff0000"}
  }
}
```

Resolved themes are cached.  Live theme reload via `{"type":"reload_theme"}`.

---

## 10. Accessibility & Internationalisation

### 10.1 Accessibility Metadata

Every render node carries an `accessible` object:

```json
{"type":"text","content":"Hostname","accessible":{"role":"label","label":"Hostname input"}}
```

Screen‑reader backend and AT‑SPI bridge are planned (v0.5+).

### 10.2 Keyboard Navigation

All widgets are fully operable by keyboard.  Tab order, focus indicators, and
accelerator keys are defined in the render tree.

### 10.3 Internationalisation

All user‑visible strings are translatable via `gettext` (planned).  The daemon
accepts a `lang` parameter.  The render tree includes locale metadata.

### 10.4 High‑Contrast & Large‑Print Themes

Built‑in high‑contrast and large‑font themes for visually impaired users
(planned).

---

## 11. Tooling & Correctness

### 11.1 Headless Test Suite

Every widget has a headless test: render initial state, inject key events via
`KEY:`, `TEXT:`, and `WAIT:` directives, assert final response matches expected
values.  119 behavioural tests cover all 33 built‑in widgets plus 9 ArtixForge
and 6 GForge plugin widgets.  Runs on every commit.

### 11.2 Snapshot Testing

ANSI output of every widget in every theme is captured as a reference file.
CI diffs against the reference.  Any rendering change is intentional (planned).

### 11.3 Fuzzing

The protocol parser is fuzzed with `libFuzzer` (planned).

### 11.4 Static Analysis

Zero warnings with `gcc -std=c99 -Wall -Wextra -O2`.  `clang-tidy`, `cppcheck`,
and MISRA C:2012 compliance planned for v0.5.

### 11.5 Memory Safety

- All external input bounds‑checked via cJSON and schema validation.
- Arena allocator per frame planned for v0.5.
- Valgrind/ASan clean enforced in CI (planned).

---

## 12. Client Libraries & Shell Wrappers

### 12.1 Bash (`fil.sh`)

Drop‑in replacement for `dialog`/`gum`.  Every wrapper follows `filly_<widget>`.

### 12.2 Bash Graphical (`filly_graphical.sh`)

Drop‑in replacement for `fil.sh` dispatching to the Python GTK4 backend.

### 12.3 Python (`filly.Session`)

Context manager with idiomatic API (planned).

### 12.4 Go (`go-filly`)

Unix socket client library (planned).

### 12.5 Node.js (`node-filly`)

npm package with async/await support (planned).

---

## 13. The FIL Scripting Language

**FIL** (FILLY Interpreted Language) is a tiny, English‑like scripting language
embedded in the daemon.  It is used for widget validation, conditional
visibility, state transformations, and any user‑defined logic that does not
require a full programming language.

**Design goals:** zero dependencies, POSIX‑compliant C implementation,
sandboxed execution, human‑readable syntax.

### 13.1 Grammar

```
script         = statement*
statement      = conditional | assignment | action | loop
conditional    = "when" expression "then" statement* ("else" statement*)? "end"
assignment     = ("let"|"set") identifier "to" expression
action         = "reject" "with" string | "accept" | "show" "when" expression
loop           = "for" "each" identifier "in" identifier "do" statement* "end"
expression     = comparison (("and"|"or") comparison)*
comparison     = value (("is"|"is not"|"does not match"|"matches"|"is less than"|"is greater than") value)?
value          = "not"? primary
primary        = identifier | string | number | "empty" | function_call | "(" expression ")"
function_call  = identifier "of" value ("with" value)?
identifier     = [a-zA-Z_][a-zA-Z0-9_]*
string         = '"' [^"]* '"'
number         = [0-9]+
```

### 13.2 Keywords

`when`, `then`, `else`, `end`, `let`, `set`, `to`, `reject`, `with`, `accept`,
`show`, `for`, `each`, `in`, `do`, `is`, `not`, `matches`, `and`, `or`,
`empty`, `of`, `less`, `greater`, `than`, `length`, `uppercase`, `lowercase`,
`trim`, `number`, `match`.

### 13.3 Data Model

All values are strings.  Boolean operations treat any non‑empty string as true,
the empty string `""` as false.  String comparisons are lexicographic.
Integer arithmetic is performed by converting to `int` internally for the
duration of the operation.

### 13.4 Built‑in Functions

| Function | Description |
|----------|-------------|
| `length of STRING` | Number of characters |
| `uppercase of STRING` | Convert to upper case |
| `lowercase of STRING` | Convert to lower case |
| `trim of STRING` | Remove leading/trailing whitespace |
| `number of STRING` | Number of elements in a JSON array string (0 if not an array) |
| `match of STRING with PATTERN` | POSIX extended regex match (returns `"1"` or `""`) |

### 13.5 Integration

FIL scripts are embedded in widget parameters:

```json
{
  "widget": "input",
  "params": {
    "title": "Hostname",
    "validation_script": "when length of value is less than 3 then reject with \"Too short\" end\naccept"
  }
}
```

Scripts may also be stored in files and referenced via `"script_file"`.

The daemon exposes the following variables to every script:

| Variable | Value |
|----------|-------|
| `value` | The current widget value |
| `store.KEY` | Any session store key, e.g. `store.DISK` |

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

## 14. Future‑Proofing & Roadmap

### v0.5 — filly‑gcore Native Graphical Backend

- DRM/KMS framebuffer rendering with stb_truetype text.
- GPU‑accelerated pixel output from the existing RenderTree.
- libinput keyboard/mouse event translation to KeyCode enum.
- Removes Python GTK4 dependency.

### v0.6+ — Advanced Features

- **Memory‑mapped IPC** — high‑throughput streaming between daemon and client.
- **Widget diffing** — send only changed screen regions during re‑render.
- **Sandboxed plugins** — `fork` + `seccomp` around plugin widget execution.
- **Self‑updating binary** — hot‑reload via `execve` without dropping clients.
- **Fault injection** — simulate dropped bytes, malloc failures, slow clients.
- **Declarative UI compilation** — JSON DSLs compiled to widget trees at build
  time, producing `.filly` bundles.
- **Macro recording & time‑travel debugging** — record/replay session events.
- **Context‑aware widget resolution** — `"widget":"auto"` selects best plugin
  widget for a context.
- **Hot‑reload plugins** — `dlclose`/`dlopen` without daemon restart.
- **Repository protocol** — HTTP API for discovering and installing plugins.
- **Remote rendering proxy** — TCP+TLS remote access with certificate pinning.
- **Snapshot testing** — ANSI output diffs in CI.
- **Fuzzing** — `libFuzzer` against protocol parser.
- **Performance benchmarks** — keystroke‑to‑screen latency regression tests.
- **Arena allocator** — per‑frame allocation, no per‑widget `malloc`/`free`.
- **Screen‑reader backend** — walk render tree, speak via Speech Dispatcher.
- **Internationalisation** — `gettext` integration.
- **High‑contrast & large‑print themes**.