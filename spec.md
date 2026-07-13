# FILLY Specification

Complete specification for the FILLY protocol, widget schema, daemon behavior,
public APIs, client libraries, and backend architecture.

---

## Table of Contents

- [Protocol](#protocol)
- [Widgets](#widgets)
- [CLI](#cli)
- [Shell Library](#shell-library)
- [Python Bindings](#python-bindings)
- [Go Client](#go-client)
- [Node.js Client](#nodejs-client)
- [Themes](#themes)
- [Plugin System](#plugin-system)
- [Accessibility](#accessibility)
- [Backends](#backends)
- [Dependencies](#dependencies)

---

# Protocol

FILLY communicates using **newline-delimited JSON (NDJSON)** over either:

- Unix sockets (daemon mode)
- Standard input/output (`stdin` / `stdout`) (oneshot mode)

Each interaction consists of **one request** followed by **one response.

---

## Request

```json
{
  "widget": "menu",
  "params": {
    "title": "Desktop",
    "message": "Pick one",
    "choices": ["KDE", "XFCE", "Budgie"]
  },
  "session_id": null
}
```

### Fields

| Field | Type | Required | Description |
|-------|------|:--------:|-------------|
| `widget` | `string` | ✅ | Widget identifier |
| `params` | `object` | No | Widget-specific parameters |
| `step` | `integer` | No | Current installer step |
| `total` | `integer` | No | Total installer steps |
| `session_id` | `string` | No | Persistent daemon session |

---

## Response

```json
{
  "result": "XFCE",
  "cancelled": false
}
```

### Fields

| Field | Type | Description |
|-------|------|-------------|
| `result` | `any` | Widget return value (`null` if cancelled) |
| `cancelled` | `boolean` | Whether the dialog was cancelled |
| `error` | `string` | Optional error message |

---

## Streaming

Certain widgets (for example **Progress**) can emit intermediate updates before
their final response.

Streaming responses are prefixed with:

```text
yield:
```

State subscriptions are prefixed with:

```text
state:
```

---

## Control Messages

| Command | Description |
|----------|-------------|
| `quit` | Shut down the daemon |
| `reload_theme` | Reload the active theme |
| `create_session` | Create a persistent session |
| `destroy_session` | Destroy an existing session |

---

# Widgets

All 33 widget types share the same JSON protocol across terminal and graphical
backends. Widget parameters are documented in the table below.

## Input Widgets

| Widget | Key Parameters |
|--------|---------------|
| `input` | `title`, `message`, `default`, `placeholder`, `validation` |
| `password` | `title`, `message`, `placeholder` |
| `form` | `title`, `fields[]`, `submit_label` |
| `text` (editor) | `title`, `file`, `content` |
| `file_picker` | `title`, `start_dir`, `filter` |

## Selection Widgets

| Widget | Key Parameters |
|--------|---------------|
| `menu` | `title`, `message`, `choices[]`, `default` |
| `yesno` | `title`, `message`, `default` |
| `checklist` | `title`, `message`, `choices[]`, `min`, `max`, `default[]` |
| `multiselect` | `title`, `message`, `choices[]`, `placeholder`, `min`, `max` |
| `filter` | `title`, `message`, `choices[]`, `placeholder` |
| `radio_group` | `title`, `message`, `choices[]`, `default` |
| `calendar` | `title` |
| `color_picker` | `title` |
| `range_slider` | `title`, `min`, `max`, `value`, `label` |

## Display Widgets

| Widget | Key Parameters |
|--------|---------------|
| `msg` | `title`, `message` |
| `summary` | `title`, `message`, `file` |
| `notification` | `message`, `duration` |
| `badge` | `text`, `color` |
| `tooltip` | `text` |
| `rich_text` | `content` |
| `spinner` | `message` |
| `separator` | `orientation` |
| `gauge` | `title`, `percent`, `label` |

## Container Widgets

| Widget | Key Parameters |
|--------|---------------|
| `hub` | `title`, `categories[]`, `actions[]` |
| `tabs` | `title`, `tabs[]` |
| `split_panes` | `orientation` |
| `table` | `title`, `headers[]`, `rows[][]` |
| `tree` | `title`, `nodes[]` |
| `context_menu` | `items[]` |

## Advanced Widgets

| Widget | Key Parameters |
|--------|---------------|
| `progress` | `title`, `command[]`, `logfile` |
| `disk` | `title`, `disk`, `partitions[]`, `free_space[]`, `readonly` |
| `button` | `title`, `label` |
| `toggle` | `title`, `label`, `default` |

---

# CLI

| Command | Description |
|----------|-------------|
| `filly demo` | Run widgets in sequence |
| `filly oneshot --input file` | Process a single JSON request |
| `filly batch --input file` | Process newline-delimited JSON requests |
| `filly daemon --socket path` | Start Unix socket daemon |

---

# Shell Library

Source `fil.sh` to access shell wrappers for every widget.

```bash
source fil.sh

filly_menu "Title" "Message" "One" "Two"
filly_yesno "Proceed?" "Continue?"
```

Every wrapper follows the naming convention `filly_<widget>`.

`filly_graphical.sh` provides identical functions targeting the GTK4 graphical
backend. Both libraries share the same function signatures.

---

# Python Bindings

```python
import filly

with filly.Session() as ui:
    result = ui.menu("Title", "Message", ["Option 1", "Option 2"])
    print(result)
```

---

# Go Client

```go
import "github.com/realvolk/filly/go-filly"

session, _ := filly.NewSession("/tmp/filly.sock")
defer session.Close()
result, _ := session.Menu("Title", "Message", []string{"A", "B"})
```

---

# Node.js Client

```javascript
const { Session } = require("filly");
const session = new Session();
await session.connect();
const result = await session.menu("Title", "Message", ["A", "B"]);
session.close();
```

---

# Themes

Themes are JSON files stored in the `themes/` directory. Built-in themes:

- Forge
- Monokai
- Solarized Dark
- Dracula
- Nord
- Gruvbox Dark
- Catppuccin Mocha
- Tokyo Night

Select a theme with `--theme <name>` or `export FILLY_THEME=<name>`.

---

# Plugin System

Plugins are shared libraries (`.so` files) located in:

```text
~/.config/filly/plugins/
```

Each plugin exports a `register_plugins` function. The daemon and oneshot mode
load all plugins automatically at startup.

Official plugin packs:

- **ArtixForge** — installer hub, recovery, ISO builder, init/desktop migration, password confirm, user manager
- **GForge** — Gentoo stage3 picker, profiles, USE flags, CFLAGS, portage configuration

---

# Accessibility

GUI widgets expose accessibility information through AT-SPI via GTK's
built-in accessibility support. Terminal widgets retain accessibility
metadata in their render tree for future screen-reader integration.

---

# Backends

## Terminal

- Raw ANSI escape sequences with 256-color support
- Unicode box drawing (single, double, rounded borders)
- Text wrapping with word break
- Scrollable lists with selected highlight
- Mouse support via terminal escape sequences
- Incremental rendering via dirty flag system

## Graphical

- GTK4 + libadwaita (Python)
- Wayland and X11 support
- Desktop notifications via `notify-send`
- Same JSON protocol as terminal backend

## Headless

- In-memory render buffer
- Programmatic event injection
- Snapshot testing and CI validation

---

# Dependencies

## Build

- C compiler (gcc or clang)
- GNU Make
- cJSON (vendored)
- Python 3 *(graphical backend only)*
- GTK4 + libadwaita *(graphical backend only)*

## Runtime

### Terminal

Only the standalone `filly` binary is required.

### Graphical

Requires:

- GTK4
- libadwaita
- PyGObject