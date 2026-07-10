# FILLY Specification

Complete specification for the FILLY protocol, widget schema, daemon behavior,
public APIs, client libraries, and backend architecture.

---

## Table of Contents

- [Protocol](#protocol)
  - [Request](#request)
  - [Response](#response)
  - [Streaming](#streaming)
  - [Control Messages](#control-messages)
- [Widgets](#widgets)
  - [Input Widgets](#input-widgets)
  - [Selection Widgets](#selection-widgets)
  - [Display Widgets](#display-widgets)
  - [Container Widgets](#container-widgets)
  - [Advanced Widgets](#advanced-widgets)
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

- Unix sockets
- Standard input/output (`stdin` / `stdout`)

Each interaction consists of **one request** followed by **one response**.

Binary mode (MessagePack) is automatically enabled when the first received byte
is not valid JSON.

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

## Input Widgets

### Input

Single-line text entry.

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `title` | string | `""` | Title text |
| `message` | string | `""` | Message text |
| `default` | string | `null` | Initial text |
| `placeholder` | string | `null` | Placeholder text |
| `validation` | string | `null` | Regex validation pattern |

### Password

Masked text entry.

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `title` | string | `""` | Title text |
| `message` | string | `""` | Message text |
| `placeholder` | string | `null` | Placeholder text |

### Form

Multiple fields in one screen.

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `title` | string | `""` | Title text |
| `fields` | array[object] | `[]` | Form field definitions |
| `submit_label` | string | `"Submit"` | Submit button text |

Each field object:

```json
{
  "label": "Username",
  "widget_type": "input",
  "value": "",
  "choices": [],
  "placeholder": "Enter username"
}
```

### Text Editor

Full-screen text editor with clipboard support.

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `title` | string | `""` | Title text |
| `file` | string | `null` | File path to edit |
| `content` | string | `null` | Initial inline content |

### File Picker

Filesystem browser.

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `title` | string | `""` | Title text |
| `start_dir` | string | `"/"` | Starting directory |
| `filter` | string | `""` | File extension filter |

---

## Selection Widgets

### Menu

Single-selection list.

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `title` | string | `""` | Title text |
| `message` | string | `""` | Message text |
| `choices` | array[string] | `[]` | List of options |
| `default` | string | `null` | Pre-selected option |

### Yes/No

Boolean confirmation dialog.

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `title` | string | `""` | Title text |
| `message` | string | `""` | Message text |
| `default` | boolean | `null` | Default choice |

### Checklist

Multiple selection with toggles.

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `title` | string | `""` | Title text |
| `message` | string | `""` | Message text |
| `choices` | array[string] | `[]` | List of options |
| `min` | integer | `0` | Minimum selections |
| `max` | integer | `null` | Maximum selections |
| `default` | array[string] | `[]` | Pre-selected options |

### Multiselect

Searchable checklist with filtering.

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `title` | string | `""` | Title text |
| `message` | string | `""` | Message text |
| `choices` | array[string] | `[]` | List of options |
| `placeholder` | string | `"Type to filter..."` | Search prompt |
| `min` | integer | `0` | Minimum selections |
| `max` | integer | `null` | Maximum selections |

### Filter

Searchable single-selection list.

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `title` | string | `""` | Title text |
| `message` | string | `""` | Message text |
| `choices` | array[string] | `[]` | List of options |
| `placeholder` | string | `"Type to filter..."` | Search prompt |

### Radio Group

Single-selection radio buttons.

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `title` | string | `""` | Title text |
| `message` | string | `""` | Message text |
| `choices` | array[string] | `[]` | List of options |
| `default` | integer | `0` | Pre-selected index |

### Calendar

Month grid date picker. Returns `YYYY-MM-DD` format.

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `title` | string | `""` | Title text |

### Color Picker

True-color RGB picker. Returns hex string like `#ff00aa`.

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `title` | string | `""` | Title text |

### Range Slider

Numeric range selector.

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `title` | string | `""` | Title text |
| `min` | integer | `0` | Minimum value |
| `max` | integer | `100` | Maximum value |
| `value` | integer | `50` | Current value |
| `label` | string | `""` | Descriptive label |

---

## Display Widgets

### Message

Informational dialog dismissed by any key.

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `title` | string | `""` | Title text |
| `message` | string | `""` | Body text |

### Summary

Scrollable text viewer.

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `title` | string | `""` | Title text |
| `message` | string | `null` | Inline content |
| `file` | string | `null` | File path to display |

### Notification

Transient toast message. Uses desktop notifications when available.

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `message` | string | `""` | Notification text |
| `duration` | integer | `3` | Auto-dismiss delay in seconds |

### Badge

Colored pill-shaped label.

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `text` | string | `""` | Badge text |
| `color` | string | `"green"` | Background color |

### Tooltip

Transient popup with descriptive text.

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `text` | string | `""` | Tooltip content |

### Rich Text

Markdown-like text with OSC 8 hyperlinks. Supports `[text](url)` syntax.

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `content` | string | `""` | Markdown content |

### Spinner

Animated loading indicator.

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `message` | string | `""` | Status message |

### Separator

Non-interactive horizontal or vertical line.

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `orientation` | string | `"horizontal"` | `"horizontal"` or `"vertical"` |

### Gauge

Standalone progress bar.

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `title` | string | `""` | Title text |
| `percent` | integer | `0` | Progress percentage (0-100) |
| `label` | string | `""` | Descriptive label |

---

## Container Widgets

### Hub

Composite installer interface with categories and inline editing.

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `title` | string | `""` | Title text |
| `categories` | array[object] | `[]` | Category definitions |
| `actions` | array[string] | `[]` | Footer action buttons |

Category object:

```json
{
  "label": "System",
  "summary_template": "Host: {HOSTNAME}",
  "items": [
    {
      "id": "HOSTNAME",
      "label": "Hostname",
      "value": "artix",
      "widget": "input",
      "choices": [],
      "placeholder": "Enter hostname",
      "visible_if": {},
      "display": ""
    }
  ]
}
```

### Tabs

Tabbed container for multiple child widgets.

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `title` | string | `""` | Title text |
| `tabs` | array[string] | `[]` | Tab labels |

### Split Panes

Resizable split view for two widgets.

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `orientation` | string | `"horizontal"` | `"horizontal"` or `"vertical"` |

### Table

Sortable data grid with column selection.

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `title` | string | `""` | Title text |
| `headers` | array[string] | `[]` | Column names |
| `rows` | array[array[string]] | `[]` | Data rows |

### Tree

Expandable/collapsible node tree.

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `title` | string | `""` | Title text |
| `nodes` | array[object] | `[]` | Tree node definitions |

Node object:

```json
{
  "label": "Root",
  "expanded": false,
  "children": []
}
```

### Context Menu

Popup menu with selectable items.

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `items` | array[string] | `[]` | Menu entries |

---

## Advanced Widgets

### Progress

Runs a command while displaying live output with a progress bar.

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `title` | string | `""` | Title text |
| `command` | array[string] | `[]` | Command and arguments |
| `logfile` | string | `null` | Optional output log path |

### Disk

Visual disk partition editor with type/flag/resize support.

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `title` | string | `""` | Title text |
| `disk` | string | `""` | Device path |
| `partitions` | array | `[]` | Partition data |
| `free_space` | array | `[]` | Free space regions |
| `readonly` | boolean | `false` | Disable editing |

### Button

Single interactive button.

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `title` | string | `""` | Title text |
| `label` | string | `""` | Button label |

### Toggle

On/off toggle switch.

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `title` | string | `""` | Title text |
| `label` | string | `""` | Toggle label |
| `default` | boolean | `false` | Initial value |

---

# CLI

| Command | Description |
|----------|-------------|
| `filly demo [--theme name]` | Run every widget in sequence |
| `filly oneshot --input file` | Process a single JSON request |
| `filly batch --input file` | Process newline-delimited JSON requests |
| `filly daemon --socket path [--theme name]` | Start Unix socket daemon |
| `filly validate --input file` | Validate a JSON request |
| `filly schema [--widget name]` | Print JSON Schema for one or all widgets |
| `filly new-widget name` | Scaffold a new widget source file |

---

# Shell Library

Source `fil.sh` to access shell wrappers for every widget.

```bash
source fil.sh

filly_menu "Title" "Message" "One" "Two"
filly_yesno "Proceed?" "Continue?"
```

Every wrapper follows the naming convention `filly_<widget>`. The library
auto-starts the daemon when required and handles JSON serialization
transparently.

`filly_graphical.sh` provides identical functions targeting the GTK4 graphical
backend. Both libraries share the same function signatures, allowing backends
to be swapped by sourcing a different file.

---

# Python Bindings

```python
import filly

with filly.Session() as ui:
    result = ui.menu(
        "Title",
        "Message",
        ["Option 1", "Option 2"],
    )
    print(result)
```

---

# Go Client

```go
import "github.com/realvolk/filly/go-filly"

session, _ := filly.NewSession("/tmp/filly.sock")
defer session.Close()

result, _ := session.Menu(
    "Title",
    "Message",
    []string{"A", "B"},
)
```

---

# Node.js Client

```javascript
const { Session } = require("filly");

const session = new Session();
await session.connect();

const result = await session.menu(
    "Title",
    "Message",
    ["A", "B"],
);

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

Select a theme with `--theme <name>` or `export FILLY_THEME=<name>`. Themes
can be reloaded at runtime via the `reload_theme` control message.

---

# Plugin System

Plugins are shared libraries (`.so` files) located in:

```text
~/.config/filly/plugins/
```

Each plugin exports a `register(registry)` function. The daemon loads all
plugins automatically at startup.

Official plugin packs:

- **ArtixForge** — installer hub, recovery, ISO builder, init/desktop migration
- **GForge** — Gentoo stage3 picker, profiles, USE flags, portage configuration

---

# Accessibility

GUI widgets expose accessibility information through AT-SPI via GTK's
built-in accessibility support. Terminal widgets retain accessibility
metadata (`role`, `label`, `description`) in their render tree for future
screen-reader integration.

---

# Backends

## Terminal (`filly-terminal`)

- Crossterm + Ratatui 0.29
- Unicode box drawing
- Mouse click and scroll
- 256-color and true-color support
- Clipboard paste via external commands (wl-paste, xclip)
- Incremental rendering via dirty flag system

## Graphical (`filly-graphical`)

- GTK4 + libadwaita
- Wayland and X11 support
- Desktop notifications via `notify-send`
- Native color picker (Gtk.ColorChooserDialog)
- Native range slider (Gtk.Scale)
- Same JSON protocol as terminal backend

## Headless (`filly-core`)

- In-memory render buffer
- Programmatic event injection
- Snapshot testing and CI validation

---

# Dependencies

## Build

- Rust (stable)
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