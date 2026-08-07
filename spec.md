# FILLY Specification — v2.0.0

**FILLY** is a universal UI server for operating system deployment, configuration,
and any interactive terminal or graphical workflow. It speaks a formal,
versioned protocol over Unix sockets, renders to terminals, graphical surfaces,
and in-memory buffers. FILLY ships its own C client library, visual GUI builder,
command-line tools, animation engine, and plugin system. The core library fits in
approximately 20,000 lines of C with zero external runtime dependencies beyond
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
18. [Performance Budgets](#18-performance-budgets)
19. [Future-Proofing & Roadmap](#19-future-proofing--roadmap)
20. [ForgeLFS Plugin Pack](#20-forgelfs-plugin-pack)
21. [Change Log](#21-change-log)

---

## 1. Protocol Fundamentals

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
{"type":"handshake","version":2,"encoding":"msgpack","capabilities":["animations","vectors","shadows","streaming"]}
```

When MessagePack is negotiated, all responses from the daemon are encoded
as MessagePack binary. The C client library handles transparent decoding.

**Capability negotiation (v2.0+):** The client declares supported features. The
daemon responds with a negotiated set. Unsupported features degrade gracefully.

| Capability | Description |
|-----------|-------------|
| `animations` | Smooth property interpolation |
| `vectors` | SVG-style path rendering |
| `shadows` | Drop shadows on containers |
| `streaming` | Incremental frame deltas |
| `positioning` | Free widget positioning |
| `flexbox` | Flexbox layout container |
| `grid` | CSS Grid layout container |
| `shaping` | Kerning, ligatures, RTL text |
| `transforms` | GPU scale/rotate/translate |
| `accessibility` | AT-SPI screen reader bridge |

### 1.3 Schema Validation

Every incoming message is validated against a JSON Schema before dispatch.
Unknown fields are rejected; malformed messages receive a detailed error
response. Implemented in `protocol/schema.c`.

### 1.4 Message Framing

All messages are single-line JSON objects terminated by `\n`. Binary payloads
(such as draw data) are preceded by a JSON header containing a `len` field,
followed by exactly `len` raw bytes and a trailing newline. The C client
library handles draw-frame reassembly.

### 1.5 Incremental Frame Streaming (v2.0)

Instead of full-frame redraws, the daemon can send delta frames:

```json
{"type":"stream_start","frame_id":1}
{"type":"stream_frame","nodes":[{"id":"widget-3","rect":{"x":10,"y":5,"w":80,"h":1},"dirty":true}]}
{"type":"stream_end","frame_id":1}
```

Only dirty nodes are transmitted. The client composites deltas onto its
existing tree.

### 1.6 Widget State Query (v2.0)

Clients can query the current value of any widget:

```json
{"type":"query","widget_id":"my-input"}
→ {"type":"query_response","widget_id":"my-input","value":"hello"}
```

### 1.7 Binary Frame Format (v2.0)

For GPU backends, frames can be sent as MessagePack-encoded binary instead of
NDJSON, reducing latency for high-frequency updates (animations, mouse drag).

### 1.8 Position Updates (v2.0)

```json
{"type":"position","widget_id":"my-widget","anchor":"top-right","dx":-2,"dy":1}
```

---

## 2. Message Catalog

### 2.1 Lifecycle Messages

| type | Direction | Purpose |
|------|-----------|---------|
| `handshake` | Client→Daemon | Negotiate encoding and capabilities |
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
| `position` | Client→Daemon | Update widget position/anchor constraints |

### 2.2 Rendering Messages

| type | Direction | Purpose |
|------|-----------|---------|
| `draw` | Daemon→Client | Frame data follows (length-prefixed) |
| `yield` | Daemon→Client | Intermediate progress update |
| `response` | Daemon→Client | Final result of a widget request |
| `stream_start` | Daemon→Client | Begin incremental frame |
| `stream_frame` | Daemon→Client | Dirty node delta |
| `stream_end` | Daemon→Client | End incremental frame |
| `query` | Client→Daemon | Query widget state |
| `query_response` | Daemon→Client | Widget state result |

### 2.3 Widget Requests (Backward Compatible)

No `"type"` field; presence of `"widget"` identifies it. Fields `relay`,
`headless`, `gui`, `session_id`, `tty`, `position`, `anchor`, `z_index`,
`overflow`, `relative_to`, `dx`, `dy` are all supported (see §5).

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
- Validates plugin Ed25519 signatures against pinned trusted keys.
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
- Rate limiting: configurable max connections per second (default 10).

### 3.3 Crash Resilience

Periodic checkpoint of active sessions to `~/.cache/filly/checkpoint.json`
(`0600`). Sensitive keys (pass, LUKS, token, key, secret) excluded from
serialization. Restored on daemon restart.

### 3.4 Self-Restart

Sending `SIGHUP` to the daemon triggers a self-restart via `execve`,
preserving the open socket file descriptor so existing connections are
not dropped.

### 3.5 Shared Memory IPC

A 16 MB POSIX shared memory region (`/filly_shm`) is created at daemon
startup for zero-copy data transfer between the daemon and GUI clients.

---

## 4. Backends

### 4.1 Terminal Backend

- Raw ANSI escape sequences, 24-bit true colour.
- Synchronised output via `\033[?2026h`.
- Incremental rendering with per-row diffing against previous frame buffer.
- Mouse support via SGR extended coordinates.
- Clipboard copy via OSC 52. Paste from system clipboard via session-level
  Ctrl+V handling.
- TUI animation subset: opacity dimming, border colour cycling, slide-in,
  fade, typewriter reveal (v2.0).
- Free widget positioning in character-cell coordinates (v2.0).
- Unicode width handling: CJK 2-cell, combining characters, RTL (v2.0).
- Terminal capability auto-detection: true color, mouse protocols, kitty
  graphics, sixel, Unicode version (v2.0).
- Inline images via kitty protocol or sixel when supported (v2.0).

### 4.2 `filly-gcore` — Native Graphical Backend

**Vector Rendering (v2.0):**
The GPU renderer supports SVG-style path strings with fill, stroke, dashes,
caps, joins, linear and radial gradients. Paths are tessellated into triangles
on the GPU or rasterized on CPU with 4x super-sampling anti-aliasing.

**Font Rendering (v2.0):**
Text rendering uses a proper shaping engine (HarfBuzz or minimal internal
alternative) for kerning, ligatures, bidirectional text, and complex script
support. Glyphs are cached in a GPU texture atlas for efficient rendering.

**Visual Features (v2.0):**
- Drop shadows with configurable offset, blur radius, spread, and color.
  Multiple shadows per widget.
- Linear and radial gradients for backgrounds.
- Backdrop blur for modal overlays.
- Border styles: solid, dashed, dotted, double, groove, ridge, inset, outset.
- Per-side independent border width and color.
- 4x super-sampling anti-aliasing on all primitives.
- GPU-accelerated animations via model matrix transforms.
- Damage-region tracking for incremental rendering.
- VSync-locked frame timing with automatic quality degradation.

**Input Handling (v2.0):**
- Keyboard, mouse, touch, and gamepad events via libinput.
- Drag and drop with visual preview.
- Resize handles on containers.
- Context menus at cursor position.
- Tooltips with hover delay.
- Focus ring and tab-order navigation.

**Output Targets:**

| Target | Status |
|--------|--------|
| DRM/KMS | Implemented |
| X11 | Implemented |
| Wayland | Implemented |
| Headless Pixel | Implemented |

### 4.3 Headless Backend

Renders to in-memory character and pixel buffers. Accepts synthetic key events.
Used for automated testing. Pixel buffer mode available via
`headless_pixel_vtable`.

### 4.4 Multi-Surface Rendering

`handle_gui_client` creates an array of backends (GCore + optional Terminal
fallback) and passes them to `session_run_multi`.

---

## 5. Widget System

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
- `flex.children`, `flex.direction`, `flex.wrap`, `flex.justify`, `flex.align` for `RNODE_FLEX` (v2.0)
- `grid.children`, `grid.columns`, `grid.rows` for `RNODE_GRID` (v2.0)
- `vector.path`, `vector.fill`, `vector.stroke` for `RNODE_VECTOR` (v2.0)
- `rich_text.spans` for `RNODE_RICH_TEXT` (v2.0)

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
   children according to their `weight` values. `RNODE_FLEX` and `RNODE_GRID`
   use CSS-style layout algorithms (v2.0).
4. **Surface constraints** — nodes are clamped to the available surface area.

### 5.3 Free Positioning and Anchoring (v2.0)

Widgets can declare a **position constraint** that replaces the default
center-locked behaviour. Three modes are supported:

1. **Anchor mode** — the widget snaps to one of nine anchor points on the
   surface: `top-left`, `top-center`, `top-right`, `center-left`, `center`,
   `center-right`, `bottom-left`, `bottom-center`, `bottom-right`.
2. **Absolute mode** — the widget is placed at fixed `x`, `y` coordinates
   (pixels in graphical mode, character cells in terminal mode).
3. **Relative mode** — the widget is offset from its parent container or
   another widget by a given `dx`, `dy`.

**Widget request fields:**

| Field | Type | Description |
|-------|------|-------------|
| `anchor` | string | Anchor point name (e.g. `"top-right"`) |
| `x`, `y` | integer | Absolute position (overrides anchor) |
| `relative_to` | string | Widget ID to position relative to |
| `dx`, `dy` | integer | Offset from relative widget or anchor |
| `z_index` | integer | Stacking order (higher = on top) |
| `overflow` | string | `"clip"`, `"visible"`, or `"scroll"` |

### 5.4 Widget Interactions (v2.0)

- **Drag and drop**: `draggable: true` on widgets enables mouse drag to
  reposition. `drop_target: true` on containers enables accepting dropped
  widgets. Events: `drag_start`, `drag_move`, `drag_end`, `drop`.
- **Resize handles**: Containers with `resizable: true` show 8-point resize
  handles. Minimum size constraints enforced.
- **Mouse cursor**: Changes based on context — `default`, `pointer`, `text`,
  `move`, `resize-*`, `not-allowed`.
- **Focus system**: Tab order based on `tab_index`. Focus ring drawn around
  active widget. Focus groups for container isolation.
- **Context menus**: Right-click triggers `contextmenu` event. Widget can
  return a menu definition to display at cursor position.
- **Tooltips**: `tooltip` field on any widget. Displays on hover after 500ms
  delay with smart placement.

### 5.5 Widget Composition

Widgets may contain child widgets recursively via `tabs` and `split_panes`
(with three-pane support via `split_panes.third`). The install hub widget
supports sub-widget dispatch for inline editing.

### 5.6 Embedded FIL Scripting

FIL scripts wired to widget validation (with store access), style definitions,
animation control, and keybinding maps.

### 5.7 Widget Lifecycle & State Machine

`EVENT_RESULT_RESPONSE` and `EVENT_RESULT_HANDLED` implemented.

### 5.8 Undo/Redo Stack

Implemented. `Ctrl+Z` / `Ctrl+Y` handled at session level.

### 5.9 Special Widgets

**Terminal Emulator Widget** — embeds a PTY in a widget with scrollback buffer
(64KB), resize handling via TIOCSWINSZ, and `/` search filter.

**Widget Builder Widget** — visual palette-based widget composer.
Keyboard-driven (F1/F2/F3 mode switching, arrow navigation, S to save).

**Macro Recorder Widget** — UI for recording, playback, save, and load of
session macros.

### 5.10 New Widgets (v2.0)

| # | Widget | Type | Description |
|---|--------|------|-------------|
| 37 | image | Display | PNG/JPEG/BMP image with fit modes |
| 38 | canvas | Interactive | Drawable surface with FIL script control |
| 39 | markdown | Display | Rendered Markdown content |
| 40 | plot | Display | Line, bar, pie, scatter charts |
| 41 | video | Media | Video playback (GPU only) |
| 42 | forge_hub | Container | ForgeLFS build configuration hub |
| 43 | forge_anvil | Tool | ForgeLFS recipe management action hub |

### 5.11 Complete Widget List

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
| 22 | rich_text | Display | Formatted text with inline styles |
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
| 37 | image | Display | PNG/JPEG/BMP with fit modes |
| 38 | canvas | Interactive | FIL-controlled drawable surface |
| 39 | markdown | Display | Rendered Markdown content |
| 40 | plot | Display | Chart widget (line/bar/pie/scatter) |
| 41 | video | Media | Video playback (GPU backend only) |
| 42 | forge_hub | Container | ForgeLFS build configuration hub |
| 43 | forge_anvil | Tool | ForgeLFS recipe management hub |

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

`.so` files in `$HOME/.config/filly/plugins/`, loaded via `dlopen`/`dlsym`
with `RTLD_NOW | RTLD_GLOBAL` for core symbol resolution.

### 8.2 Signature Verification

Ed25519 detached signatures via libsodium. **Key pinning (v2.0):** The daemon
embeds a list of trusted public keys. Plugins must be signed by at least one
trusted key. Multiple `.sig` files per plugin are supported. Key delegation
with expiry and scope limits via a web of trust model.

### 8.3 Sandboxed Plugin Execution

Linux: seccomp. OpenBSD: pledge. FreeBSD: capsicum. **Enabled by default
(v2.0).** Plugins run in isolated processes communicating over pipes. Per-plugin
syscall manifests. Filesystem restricted to plugin directory and
`/usr/share/filly`. Network access denied by default. Hard timeout of 5 seconds
per plugin code execution.

### 8.4 ForgeLFS Plugin Pack (v2.0)

The ForgeLFS plugin (`libforgelfs.so`) provides:

- **`forge_hub`** — Full configuration hub for LFS-style system builds with
  `build_state` object for live queue progress, stage display, failure counts,
  and F-key build control shortcuts.
- **`forge_anvil`** — Action hub for recipe management, matching the ArtixForge
  anvil pattern.

### 8.5 Supply Chain Security (v2.0)

- **Plugin hash verification** — optional SHA256 hash check against known-good list.
- **Reproducible builds** — bit-for-bit identical binaries from the same source.
- **Binary transparency** — plugin hashes and signatures publishable to an
  append-only transparency log.

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
opacity, transitions, and animation transform properties.

**New in v2.0:**
- Multiple drop shadows (`shadow` array replacing single shadow fields)
- Linear and radial gradients (`bg_gradient.type`, `bg_gradient.stops`)
- Border styles per side (`border_top_style`, `border_top_color`, etc.)
- Backdrop blur (`backdrop_blur` radius in pixels)
- Font shaping properties (`letter_spacing`, `line_height`, `text_transform`)

### 9.5 Transitions

Lerp-based property interpolation on state changes. Extended to all animatable
properties including position and size (v2.0).

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
| translate_x, translate_y | GPU transform | slide animation (v2.0) |
| rect.x, rect.y, rect.w, rect.h | lerp | slide animation (v2.0) |

### 10.3 TUI Animations (v2.0)

- **Slide-in**: Widget appears to slide from an edge anchor to its final
  position by redrawing at intermediate offsets over multiple frames.
- **Fade**: Background/foreground colors interpolate through a dimmed state.
- **Typewriter reveal**: `RNODE_TEXT` nodes rendered character-by-character.
- **Pulse**: Border or background color oscillates.

### 10.4 Animation Lifecycle

- `animation_play(tree, name, now_ms)` — starts a named animation
- `animation_stop(tree, name)` — stops and removes
- `animation_pause(tree, name)` / `animation_resume(tree, name, now_ms)`
- `animation_update(tree, now_ms)` — called per frame by session loop

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
- **v2.0:** Free widget positioning mode with anchor snapping

### 11.4 Connection Graph Editor

- Node/port/edge visual diagram
- Drag wires from output ports to input ports
- Port compatibility checking (TRIGGER→any, type matching)
- Edge condition/transform editing (FIL expressions)

### 11.5 Property Editor

- Dynamic form generation from widget `ParamDesc` arrays
- Keyboard editing with field navigation
- Live parameter update triggers preview re-render

### 11.6 Code Generation

- FIL script generation from connection graph edges
- Complete C plugin source with layout builder, event handlers, factory
- Makefile output for standalone compilation

### 11.7 Validation Pipeline

13 automated checks: duplicate IDs, overlap, bounds, keyboard access,
tab order, colour contrast, FIL syntax, dangling edges, type mismatches,
cycles, missing labels, unused store vars, orphan widgets.

---

## 12. Accessibility & Internationalisation

### 12.1 Accessibility Profiles

High-contrast and large-print themes. `set_accessibility` protocol message.

### 12.2 Internationalisation

gettext initialization. `_()` macro. `.pot` template generation. RTL detection.

### 12.3 AT-SPI Bridge (v2.0)

Every `RenderTree` node with `accessible.role` and `accessible.label` is
exported to the system accessibility bus on Linux (AT-SPI). Updates are
pushed on each frame. Screen reader announces focused widget labels.

### 12.4 Keyboard Navigation (v2.0)

- Tab/Shift+Tab moves focus between widgets in tab order
- Arrow keys navigate within widgets (lists, menus, grids)
- Enter/Space activates; Escape cancels/closes
- Global shortcuts configurable via keymap

### 12.5 Reduced Motion (v2.0)

`prefers_reduced_motion` capability disables animations and transitions.
TUI ignores slide/fade entirely.

---

## 13. Tooling & Correctness

### 13.1 Headless Test Suite

140+ behavioural tests covering all widget types across terminal, headless
character, and headless pixel backends.

### 13.2 Cross-Backend Widget Tests

Every widget type tested in terminal, graphical, and headless modes.

### 13.3 Snapshot Testing

Pixel and ANSI snapshot comparison with `--generate` and `--mode` flags.

### 13.4 Fuzzing

libFuzzer harness for all message handlers. CI integration.

### 13.5 Static Analysis

`.clang-tidy` config. Makefile targets: `lint`, `cppcheck`. Zero warnings
under `-std=c99 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -pedantic`.

### 13.6 Memory Safety

Arena allocator per frame. Valgrind CI. Zero leaks in test suite.

### 13.7 Developer Tooling (v2.0)

- **Widget Inspector** — F12 opens inspector overlay showing widget tree,
  properties, and style resolution.
- **Performance Profiler** — compile with `-DFILLY_PROFILING` for frame time
  graphs, memory allocation tracking.
- **Hot Reload** — themes, plugins, and FIL scripts reload on change without
  daemon restart.
- **Error Console** — in-widget display of JSON errors, type mismatches,
  rendering errors.

---

## 14. Client Libraries, Tools & Shell Wrappers

### 14.1 C Client Library

Full API: connect, send, send_key, poll, get_response, draw callback.

### 14.2 CLI Tools

- `filly send` — one-shot daemon communication
- `filly build` — JSON constructor with template substitution
- `filly relay` — interactive TTY bridge
- `filly oneshot` — direct rendering with backend selection
- `filly compile` — compile widget JSON to binary format
- `filly update` — self-updating binary
- `filly-build` — GUI builder for visual widget composition
- `filly inspect` — widget tree inspector (v2.0)
- `filly profile` — performance profiler (v2.0)

### 14.3 Shell Wrappers

`fil.sh` for Bash integration.

### 14.4 Language Bindings

Python (ctypes), Go (cgo), Node.js (napi) linking against `libfilly.so`.

---

## 15. The FIL Scripting Language

### 15.1–15.4 Grammar, Keywords, Data Model, Built-in Functions

Fully implemented recursive-descent parser and interpreter with animation
statements.

### 15.5 Integration

Wired to widget validation (with store access), style definitions,
animation control, and keybinding maps.

### 15.6 Sandbox

1-second timeout, 10,000 loop cap, no file I/O, no network, no shell.

---

## 16. Security Model

### 16.1 Socket Security

- Unix socket only, never TCP. No network exposure.
- `0600` permissions by default.
- Peer credential verification via `filly-port/`.
- Cross-user connections rejected.

### 16.2 Plugin Trust Model (v2.0)

- Key pinning: daemon embeds trusted public keys.
- Multiple signers per plugin.
- Key delegation with expiry and scope limits.
- Optional SHA256 hash verification against known-good list.

### 16.3 Plugin Sandboxing (v2.0)

Enabled by default. Isolated processes with restricted syscall tables.
Filesystem limited to plugin directory. Network denied. 5-second hard timeout.

### 16.4 Input Validation

- Schema enforcement on all messages.
- Buffer overflow protection via arena bounds checking.
- Stack canaries, PIE, RELRO on daemon binary.
- Fuzzing coverage in CI.

### 16.5 Clipboard Security (v2.0)

- Paste filtering: only printable characters.
- No secret exfiltration — never written to disk or logged.
- First-read consent prompt for applications.

### 16.6 Audit Logging (v2.0)

Structured JSON logs for plugin loads, signature verifications, sandbox
violations. Shippable to remote collector.

### 16.7 Checkpoint Sanitisation

Sensitive keys excluded from serialization.

---

## 17. Portability

FILLY is written in **strict ISO C99** with **POSIX.1-2008** as the only
required standard. No GNU extensions, no compiler-specific attributes.

### 17.1 Compiler Requirements

- `-std=c99 -D_POSIX_C_SOURCE=200809L`
- Zero warnings under `-Wall -Wextra -pedantic`
- Tested on GCC, Clang, musl-gcc, slimcc

### 17.2 Platform Support

| Feature | Linux | FreeBSD | OpenBSD |
|---------|-------|---------|---------|
| Peer credentials | `SO_PEERCRED` | `LOCAL_PEERCRED` | `getpeereid` |
| File watching | `inotify` | `kqueue` | `kqueue` |
| Sandboxing | `seccomp` | `capsicum` | `pledge` |
| Shared memory | POSIX `shm_open` | POSIX `shm_open` | POSIX `shm_open` |
| Dynamic loading | `dlopen`/`dlsym` | `dlopen`/`dlsym` | `dlopen`/`dlsym` |
| Threads | POSIX pthreads | POSIX pthreads | POSIX pthreads |

---

## 18. Performance Budgets

### 18.1 Binary Size

- Daemon binary: <500 KB (currently 411 KB)
- libfilly.so: <1 MB with all backends
- Plugin `.so`: <200 KB each
- GPU shader cache: <2 MB

### 18.2 Memory

| Component | Budget |
|-----------|--------|
| Idle daemon | <10 MB RSS |
| Per-connection | <2 MB (arena + render tree) |
| GPU rendering | <50 MB VRAM |
| Animation state | <1 KB per animation |
| Plugin sandbox | <50 MB per process |

### 18.3 CPU

| Operation | Budget |
|-----------|--------|
| Frame render | 16 ms (60 FPS) |
| Animation update | <1 ms per frame |
| Layout pass | <2 ms (100-node tree) |
| Text shaping | <5 ms (full screen) |
| Plugin execution | <500 ms avg, 5 sec hard kill |

### 18.4 Resource Limits

| Resource | Limit | Configurable |
|----------|-------|--------------|
| Max JSON request | 1 MB | yes |
| Max arena per frame | 64 MB | yes |
| Max connections | 100 | yes |
| Connections/sec | 10 | yes |
| Max plugin processes | 8 | yes |
| Plugin memory | 256 MB | yes |

### 18.5 Terminal

- Frame size: limited to terminal dimensions
- ANSI escape budget: <10 KB per frame
- Mouse latency: <50 ms

---

## 19. Future-Proofing & Roadmap

### v2.0 (current working draft)

All sections marked (v2.0) above.

### v2.1+

- Community plugin store with review process
- Gesture and eye tracking input
- Terminal emulator: scrollback search with regex
- Macro recording: edit and merge recordings
- Distributed session sharing
- Voice input integration
- ARM aarch64 native GPU backend

---

## 20. ForgeLFS Plugin Pack

### 20.1 `forge_hub` Widget

A configuration hub for LFS-style system builds. Accepts standard
`categories` and `actions` arrays plus an optional `build_state` object:

```json
{
  "widget": "forge_hub",
  "params": {
    "title": "ForgeLFS Configuration",
    "categories": [ ... ],
    "actions": ["Proceed"],
    "build_state": {
      "stage": "toolchain",
      "current": 42,
      "total": 137,
      "failed": 1,
      "queue": ["binutils-pass1", "gcc-pass1", "linux-headers", ...]
    }
  }
}
```

F-key bindings:
- **F2**: View build log (overlay)
- **F3**: View build queue (overlay)
- **F5**: Retry failed packages
- **F10**: Abort build

Returns a JSON object of configuration key-value pairs on Proceed, or
action strings (`"retry_failed"`, `"abort_build"`) for build control.

### 20.2 `forge_anvil` Widget

An action hub for recipe management, identical in structure to the
ArtixForge `anvil` widget. Displays categories with nested actions,
returns the selected action key on confirmation.