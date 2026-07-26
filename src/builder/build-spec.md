# FILLY Builder — Specification v0.9.0

## Overview

`filly-build` is a standalone binary that provides a visual environment for
composing widget layouts, defining inter-widget connections, configuring
keyboard navigation, and generating self-contained FILLY plugin directories.
It is a FILLY application — it uses the same `session_run` loop, `RenderTree`,
and backends as any other FILLY tool. The builder runs on any active FILLY
backend (Wayland, X11, DRM, or terminal TUI), proving the toolkit's
universality.

**Core principle:** Every interactive element must be reachable and operable
without a mouse. The builder helps the user design keyboard-first interfaces,
with TUI compliance feedback, real-time validation, and integrated testing.

---

## Architecture

### Binary and dependencies

`filly-build` links against the FILLY core (`libfilly` internal objects):
- `src/core/` — widget vtable, registry, render, session, store, theme, arena,
  clipboard, undo, FIL, client, config, recorder
- `src/backend/headless/` — for live canvas preview (pixel buffer)
- `src/backend/gcore/` — font loading, pixel renderer, `gcore_render_tree_to_pixels`
- `src/script/fil.c` — FIL validation and execution
- `src/protocol/` — JSON schema validation, message parsing
- `src/cJSON.o`

It does **not** link the daemon socket code, Wayland clipboard, GPU backend, or
X11/DRM direct setup unless it's running as a GUI app on those backends.
The builder shell renders via whichever backend is active (selected like
`filly oneshot --gui` or terminal fallback).

### Source tree

```
src/builder/
  filly_build.c         — main(), root widget, shell layout, mode dispatch
  project.h / .c        — data model, I/O, manipulation API
  canvas.h / .c         — visual layout editor, live preview, drag-drop
  connection_graph.h/.c — node/port/edge editor, visual graph rendering
  property_editor.h/.c  — dynamic property form
  codegen.h / .c        — FIL generation, C code generation, Makefile
  validator.h / .c      — validation pipeline
  builder-spec.md       — this document
```

### Application shell

The builder window is a single FILLY widget (a root `RNODE_CONTAINER`) composed
of sub-panes using `split_panes` and `tabs`. The shell layout:

```
┌──────────────────────────────────────────────────────────────┐
│ Toolbar / Status bar                                       │
│ [Edit] [Wire] [Preview] [TUI-Preview]   Save Export Undo   │
├──────────┬───────────────────────────────┬──────────────────┤
│ Palette  │                               │ Properties       │
│ (list)   │         Canvas                │ (dynamic form)   │
│          │                               │                  │
│          │                               │                  │
├──────────┴───────────────────────────────┴──────────────────┤
│ Connection Graph / FIL Editor (tabs)                        │
│ [Graph Nodes] [Edges List] [FIL Source]                    │
├─────────────────────────────────────────────────────────────┤
│ Status: 12 items, 5 connections, 0 errors, 2 warnings      │
└─────────────────────────────────────────────────────────────┘
```

### Modes (F-keys and toolbar)

- **F1 / Edit mode** — canvas visible, drag-drop, resize, property editing
- **F2 / Wire mode** — connection graph visible, draw wires between ports
- **F3 / Preview mode** — full live preview of the composed UI (no builder chrome)
- **F4 / TUI preview** — ANSI rendering of the layout in a terminal-like viewport
- **F5 / Cycle panes** — move focus between palette, canvas, properties, graph

### Keyboard shortcuts

| Key        | Action                              |
|------------|-------------------------------------|
| Ctrl+S     | Save project                        |
| Ctrl+E     | Export plugin                       |
| Ctrl+Z     | Undo                                |
| Ctrl+Y     | Redo                                |
| Delete     | Delete selected item/edge/node      |
| Ctrl+C/V   | Copy/paste selected item            |
| Ctrl+A     | Select all                          |
| Arrow keys | Move selected item (1px grid)       |
| Shift+Arrows | Move selected item (10px grid)   |
| Ctrl+Arrows | Resize selected item             |
| +/-        | Zoom in/out                         |
| Ctrl+0     | Zoom to fit                         |
| Escape     | Deselect / cancel wire drag / back  |

---

## Project Data Model (`project.h`)

```c
typedef struct BuilderProject {
    char *name;
    char *file_path;
    
    // Layout items
    CanvasItem *items;
    int item_count;
    int next_id;
    int root_w, root_h;
    
    // Connection graph
    GraphNode *nodes;
    int node_count;
    ConnectionEdge *edges;
    int edge_count;
    
    // Keybindings
    KeymapEntry *keymaps;
    int keymap_count;
    
    // State variables
    int store_var_count;
    StoreVar *store_vars;
    
    // TUI configuration
    TuiConfig tui;
    
    // Metadata
    int version;          // FILLY protocol version
    char *author;
    char *description;
    
    UndoStack *undo;
} BuilderProject;
```

### CanvasItem

```c
typedef struct {
    int id;
    char *widget_type;       // registry name: "input", "menu", etc.
    char *instance_name;     // user-editable label, defaults to widget_type_N
    Rect rect;               // position and size on canvas
    cJSON *params;           // widget parameters (title, message, choices...)
    int parent_id;           // -1 = top-level; otherwise ID of container parent
    int tab_index;           // keyboard navigation order
    bool locked;             // prevent accidental moves
    bool visible;            // for show/hide logic
    
    // Runtime preview cache (not serialized)
    Widget *live_widget;
    RenderTree *cached_tree;
    bool preview_dirty;
} CanvasItem;
```

### Graph nodes and edges

```c
typedef enum { NODE_WIDGET, NODE_STORE, NODE_FIL_BLOCK, NODE_CONDITIONAL } NodeType;

typedef enum { PORT_TRIGGER, PORT_STRING, PORT_BOOL, PORT_INT } PortDataType;

typedef struct {
    int id;
    char *name;              // "on_submit", "value", "checked"
    PortDataType type;
    bool is_output;
    char *description;
} PortDef;

typedef struct GraphNode {
    int id;
    NodeType type;
    char *label;
    int widget_id;           // CanvasItem id, for NODE_WIDGET
    char *store_key;         // store variable name, for NODE_STORE
    char *fil_script;        // raw FIL, for NODE_FIL_BLOCK
    PortDef *ports;
    int port_count;
} GraphNode;

typedef struct {
    int from_node;
    int from_port;
    int to_node;
    int to_port;
    char *condition;         // FIL expression, NULL = always
    char *transform;         // FIL expression, NULL = pass-through
} ConnectionEdge;
```

### Keybindings

```c
typedef enum { KB_SCOPE_GLOBAL, KB_SCOPE_WIDGET } KeymapScope;

typedef struct {
    KeymapScope scope;
    int widget_id;           // only for KB_SCOPE_WIDGET
    char *key;               // "Enter", "Escape", "Ctrl+S", "a"
    char *action;            // "select", "back", "move-up", "custom:myAction"
    char *custom_fil;        // FIL script for custom actions
} KeymapEntry;
```

### Store variables

```c
typedef struct {
    char *key;
    char *initial_value;
    char *type;              // "string", "bool", "int"
} StoreVar;
```

### TUI configuration

```c
typedef struct {
    bool show_tab_order;
    int min_term_w, min_term_h;
    char *fallback_font;
} TuiConfig;
```

---

## Palette

The palette lists every registered widget type, grouped by category. It is
populated dynamically at startup by calling `widget_registry_enum()` to walk
the registry linked list. This includes all built-in widgets, ArtixForge
plugins, GForge plugins, and any third-party `.so` files loaded from
`~/.config/filly/plugins/`.

Categories are derived from a static mapping. Any widget not in the mapping
falls into an "Extensions" bucket with an info badge: "community widget — port
definitions may be incomplete."

```
── Input ──
  input          password        form
  text_editor    file_picker
── Selection ──
  menu          checklist       multiselect
  filter        radio_group     calendar
  color_picker  range_slider
── Display ──
  msg           summary         notification
  badge         tooltip         rich_text
  spinner       separator       gauge
── Interactive ──
  toggle        checkbox        button
── Containers ──
  tabs          split_panes     hub
── Advanced ──
  progress      disk            table
  tree          context_menu    terminal_emulator
  widget_builder macro_recorder
── Extensions ──
  <dynamically populated from plugin registry>
```

Double-clicking a palette item enters **placement mode**: cursor changes to
crosshair, next click on canvas places the widget at default size (200×120).
Dragging from palette to canvas positions and sizes in one gesture.

---

## Port Definitions per Widget Type

A static table `WidgetPortInfo port_registry[]` maps known widget types to
their available ports. The port lookup function `project_get_widget_ports()`
checks this table first, and for unknown types returns a fallback: one
`set_visible`(BOOL) input port and no outputs. A warning is emitted to the
validation report: `"Widget 'my_custom' has no port definitions — add custom
ports in the FIL editor."`

Users can manually add input/output ports to any graph node via the property
editor when the node is selected, in case a plugin widget exposes events not
in the static table.

### Built-in port definitions

| Widget | Output ports | Input ports |
|--------|-------------|-------------|
| input | `on_submit`(TRIGGER), `on_change`(TRIGGER), `value`(STRING) | `set_value`(STRING), `set_visible`(BOOL) |
| password | `on_submit`(TRIGGER), `value`(STRING) | `set_visible`(BOOL) |
| toggle | `on_toggle`(TRIGGER), `value`(BOOL) | `set_value`(BOOL), `set_visible`(BOOL) |
| checkbox | `on_check`(TRIGGER), `checked`(BOOL) | `set_checked`(BOOL), `set_visible`(BOOL) |
| menu | `on_select`(TRIGGER), `value`(STRING), `index`(INT) | `set_visible`(BOOL) |
| checklist | `on_change`(TRIGGER), `selected`(STRINGS) | `set_visible`(BOOL) |
| multiselect | `on_change`(TRIGGER), `selected`(STRINGS) | `set_visible`(BOOL) |
| filter | `on_select`(TRIGGER), `value`(STRING) | `set_visible`(BOOL) |
| radio_group | `on_select`(TRIGGER), `value`(STRING), `index`(INT) | `set_visible`(BOOL) |
| calendar | `on_select`(TRIGGER), `date`(STRING) | `set_visible`(BOOL) |
| color_picker | `on_select`(TRIGGER), `color`(STRING) | `set_visible`(BOOL) |
| range_slider | `on_change`(TRIGGER), `value`(INT) | `set_value`(INT), `set_visible`(BOOL) |
| file_picker | `on_select`(TRIGGER), `path`(STRING) | `set_visible`(BOOL) |
| text_editor | `on_save`(TRIGGER), `content`(STRING) | `set_content`(STRING), `set_visible`(BOOL) |
| form | `on_submit`(TRIGGER), `values`(JSON) | `set_visible`(BOOL) |
| progress | `on_complete`(TRIGGER), `percent`(INT) | `set_visible`(BOOL) |
| tabs | `on_tab_change`(TRIGGER), `active_tab`(INT) | `set_active_tab`(INT), `set_visible`(BOOL) |
| split_panes | `on_resize`(TRIGGER), `split_pos`(INT) | `set_visible`(BOOL) |
| table | `on_select`(TRIGGER), `value`(STRING), `row`(INT), `col`(INT) | `set_visible`(BOOL) |
| tree | `on_select`(TRIGGER), `path`(STRING) | `set_visible`(BOOL) |
| gauge | `percent`(INT) | `set_value`(INT), `set_visible`(BOOL) |
| spinner | none | `set_visible`(BOOL) |
| separator | none | `set_visible`(BOOL) |
| badge | none | `set_text`(STRING), `set_visible`(BOOL) |
| msg | none | `set_visible`(BOOL) |
| summary | none | `set_visible`(BOOL) |
| notification | `on_dismiss`(TRIGGER) | `set_visible`(BOOL) |
| tooltip | none | `set_text`(STRING), `set_visible`(BOOL) |
| rich_text | none | `set_content`(STRING), `set_visible`(BOOL) |
| context_menu | `on_select`(TRIGGER), `value`(STRING) | `set_visible`(BOOL) |
| disk | `on_change`(TRIGGER), `partitions`(JSON) | `set_visible`(BOOL) |
| hub | `on_navigate`(TRIGGER), `active_section`(STRING) | `set_visible`(BOOL) |
| terminal_emulator | `on_exit`(TRIGGER), `exit_code`(INT) | `set_visible`(BOOL) |
| widget_builder | `on_export`(TRIGGER), `layout_json`(STRING) | `set_visible`(BOOL) |
| macro_recorder | `on_save`(TRIGGER), `recording_path`(STRING) | `set_visible`(BOOL) |
| yesno | `on_yes`(TRIGGER), `on_no`(TRIGGER), `choice`(BOOL) | `set_visible`(BOOL) |
| button | `on_click`(TRIGGER) | `set_visible`(BOOL) |
| list | `on_select`(TRIGGER), `value`(STRING), `index`(INT) | `set_visible`(BOOL) |

**Store variable nodes:**
| Output | Input |
|--------|-------|
| `value`(STRING) | `set`(STRING) |

**Fallback for unknown widgets:**
| Output | Input |
|--------|-------|
| (none) | `set_visible`(BOOL) |

### User-defined ports

When a graph node is selected, the property editor shows its port list. The
user can add custom input or output ports with a name, data type, and
description. Custom ports appear in the FIL script as `custom.<port_name>`.
This allows wiring plugin widgets that expose undocumented events.

Custom ports are serialized in the `.filly-project` file:

```json
{
  "id": 5,
  "type": "widget",
  "label": "my_plugin_widget",
  "widget_id": 3,
  "ports": [
    {"id": 0, "name": "set_visible", "type": "bool", "out": false, "desc": "Auto-generated fallback"},
    {"id": 1, "name": "on_custom_event", "type": "trigger", "out": true, "desc": "User-defined: fires on custom plugin event"}
  ]
}
```

---

## Project File Format

`.filly-project` JSON:

```json
{
  "version": 1,
  "filly_version": 1,
  "name": "my_project",
  "author": "",
  "description": "",
  "root": {"w": 800, "h": 600},
  "items": [
    {
      "id": 0,
      "type": "input",
      "name": "username_input",
      "rect": {"x": 20, "y": 20, "w": 300, "h": 40},
      "params": {"title": "Username", "placeholder": "Enter username"},
      "parent": -1,
      "tab_index": 1,
      "locked": false,
      "visible": true
    }
  ],
  "nodes": [
    {
      "id": 0,
      "type": "widget",
      "label": "username_input",
      "widget_id": 0,
      "ports": [
        {"id": 0, "name": "on_submit", "type": "trigger", "out": true},
        {"id": 1, "name": "value", "type": "string", "out": true}
      ]
    },
    {
      "id": 1,
      "type": "store",
      "label": "store.username",
      "store_key": "username",
      "ports": [
        {"id": 0, "name": "value", "type": "string", "out": true},
        {"id": 1, "name": "set", "type": "string", "out": false}
      ]
    }
  ],
  "edges": [
    {"from_node": 0, "from_port": 0, "to_node": 1, "to_port": 1}
  ],
  "keymaps": [
    {"scope": "global", "key": "Enter", "action": "select"},
    {"scope": "global", "key": "Escape", "action": "back"},
    {"scope": "widget", "widget_id": 0, "key": "Ctrl+S", "action": "custom:saveInput"}
  ],
  "store_vars": [
    {"key": "username", "initial": "", "type": "string"}
  ],
  "tui": {
    "show_tab_order": true,
    "min_term_w": 80,
    "min_term_h": 24
  }
}
```

---

## Canvas System (`canvas.h`)

### Features

- Infinite zoom (0.1x to 5.0x) centered on cursor
- Pan via middle-mouse drag or scroll
- Snap-to-grid (configurable grid size, default 8px)
- Alignment guides (magnetic lines when edges align with siblings)
- Rubber-band multi-select
- Resize handles (8 points: corners + edge midpoints)
- Container nesting — dragging a widget into a container makes it a child
- Z-order (draw order) adjustable via Ctrl+Shift+Up/Down
- Minimap (optional, small overview rectangle in corner)
- Tab order overlay (numeric labels on each widget showing navigation sequence)

### Live preview

The canvas allocates a `HeadlessBackend` in pixel mode at the canvas viewport
size. On each frame:

1. Build a temporary `RenderTree` with all visible `CanvasItem`s
2. Call `resolve_node_styles()` with active theme
3. Call `gcore_render_tree_to_pixels()` into the pixel buffer
4. Blit the pixel buffer into the canvas `RenderTree` node as a texture
   (using `RNODE_CURSOR` pixel-by-pixel drawing or a GPU texture if available)

The preview is live — editing a property in the property editor immediately
triggers a re-render of that item's `live_widget`.

### Mouse handling

| Action                | Behavior                                          |
|-----------------------|---------------------------------------------------|
| Left click on item    | Select item                                       |
| Left click on canvas  | Deselect all                                      |
| Left drag on item     | Move item (or resize if near handle)              |
| Left drag on canvas   | Rubber-band select                                |
| Right click           | Context menu (delete, lock, bring to front, etc.) |
| Middle drag           | Pan canvas                                        |
| Scroll wheel          | Zoom in/out                                       |
| Ctrl+Scroll           | Scroll vertically                                 |
| Shift+Scroll          | Scroll horizontally                               |

### Coordinate system

All item coordinates (`item->rect`) are stored in **unscaled canvas space**.
The canvas viewport applies `zoom` and `scroll_x/y` for display. The live
preview renders at `canvas_w * zoom` resolution and scales down for display.

---

## Property Editor (`property_editor.h`)

### Dynamic form generation

The property editor queries the widget's `ParamDesc` array via a new public
function in `widget_factories.c`:

```c
const ParamDesc *widget_get_params(const char *widget_type, int *count);
```

Each `ParamDesc` maps to a form field:

| ParamType  | Editor widget   |
|------------|-----------------|
| P_STR      | Text input      |
| P_INT      | Number spinner  |
| P_BOOL     | Toggle/checkbox |
| P_JSON     | Sub-editor (text area with JSON syntax) |
| P_STRS     | Tag editor (add/remove string items)   |

Color fields (ending in `_color`) get a color picker pop-up.

### Live update

When a property changes, the editor calls `project_update_item_params()`, which:
1. Updates `item->params` JSON
2. Calls `item->live_widget->destroy()` if it exists
3. Creates a new live widget via `widget_registry_create()` with updated params
4. Sets `item->preview_dirty = true`

### Validation

The property editor validates values against the parameter type before
committing. Invalid values highlight the field in red and show a tooltip
with the error.

### Undo

Every property change pushes an undo action with the old and new values,
serialized as JSON. Undo restores the old params and rebuilds the live widget.

---

## Connection Graph Editor (`connection_graph.h`)

### Visual layout

The graph is rendered as a node diagram:
- **Widget nodes:** rounded rectangles with the widget name, output ports
  (circles) on the right, input ports on the left
- **Store nodes:** rounded rectangles with a database icon, store key label,
  value output port on right, set input port on left
- **FIL block nodes:** rectangle with script icon, label, input/output ports
- **Conditional nodes:** diamond shape, condition label, true/false output ports
- **Edges:** Bezier curves from output port to input port, color-coded by
  data type (trigger = red, string = green, bool = blue, int = yellow)

### Interaction

- **Click on port:** start drawing a wire (rubber-band line follows cursor)
- **Click on compatible port while wiring:** create edge, if compatible
  (output→input, matching data types or trigger→any)
- **Right-click on port while wiring:** cancel
- **Right-click on edge:** context menu (delete, edit condition/transform)
- **Double-click on edge:** open condition/transform editor (FIL expression)
- **Drag node header:** move node
- **Click on node:** select, show properties in side panel
- **Delete key:** remove selected node or edge

### Port compatibility

| Output type | Can connect to input type   |
|-------------|-----------------------------|
| TRIGGER     | Anything (fires the input)  |
| STRING      | STRING only                 |
| BOOL        | BOOL, STRING (converted)    |
| INT         | INT, STRING (converted)     |

### Auto-node creation

When a new `CanvasItem` is added, the graph editor automatically creates a
corresponding `GraphNode` with ports from the `WidgetPortInfo` table.

When a `store.key` is referenced in a FIL script or connection, a store node
is automatically created if it doesn't exist.

### Graph validation

- **Dangling output:** Warning — port not connected to anything
- **Type mismatch:** Error — trying to connect STRING to BOOL without transform
- **Cycle:** Error — A→B→A loop
- **Orphan node:** Info — node with no connections

---

## FIL Generation from Connections

The connection graph is translated to FIL scripts by walking the graph in
topological order (triggers first, then data flow edges).

### Example

Graph edges:
- `input.username.on_submit` → `store.username.set` (with transform: `value of username`)
- `input.username.on_submit` → `msg.welcome.show` (no condition)
- `store.username.value` → `input.username.set_value` (keeps input in sync)

Generated FIL:
```
-- Connections from username_input.on_submit
when widget.username_input.submit then
  set store.username to value of username_input
  show msg.welcome
  accept
end

-- Data binding: store.username -> username_input.value
when store.username changes then
  set value of username_input to store.username
end
```

### FIL block nodes

Users can add raw FIL script nodes to the graph for custom logic. These
appear as nodes with input/output ports defined by the user. The script is
embedded directly in the generated output.

### Validation

After generation, the FIL script is run through `fil_eval()` with a dummy
store to detect syntax errors. Errors are highlighted in the FIL editor
pane with line numbers.

---

## TUI Compliance

### Tab order overlay

In edit mode, pressing `Ctrl+T` toggles a tab order overlay on the canvas.
Each widget displays its `tab_index` as a numbered badge. The user can click
to reorder or use `Ctrl+Shift+Up/Down` to move the selected widget's position
in the order.

### Keyboard accessibility rules

The validator enforces:
1. Every interactive widget (input, button, menu, toggle, checkbox, etc.)
   must have a `tab_index` set.
2. No two widgets share the same `tab_index`.
3. All trigger ports (`on_submit`, `on_click`, `on_select`) must have at
   least one keyboard binding in the keymap (Enter/Space default, or custom).
4. No mouse-only actions — every connection must be triggerable by a key event.

### TUI preview mode (F4)

Renders the composed layout using the terminal backend (`headless_vtable` with
ANSI drawing). The output is captured as a string and displayed in a scrollable
text viewport within the builder. The user can resize the viewport to test
different terminal dimensions.

Key bindings work in the preview: the builder routes keystrokes to the
headless session, so the user can interact with the TUI version of their
design.

### TUI-specific validation

- **Minimum terminal size:** Warn if any widget rect extends beyond the
  configured `min_term_w` × `min_term_h`.
- **Color contrast:** ANSI 256-color mode may lose distinction for certain
  color pairs. The validator simulates all 8 built-in themes against a
  16-color terminal palette and flags unreadable combinations.
- **Font width:** CJK and wide characters may overflow. Warn if `font_family`
  is set to a variable-width font.
- **Border style:** `BORDER_ROUNDED` is recommended over `BORDER_DOUBLE` for
  TUI compatibility (some terminals don't render double borders).

---

## Code Generation (`codegen.h`)

### Output structure

```
my_plugin/
├── my_plugin.c          # Generated C widget source
├── Makefile              # Build instructions
└── my_plugin.json        # Optional schema for the plugin
```

### C file structure

1. **Includes** — all needed FILLY core headers, widget-specific headers for
   each widget type used
2. **Layout builder** — `static RenderTree *build_layout(Arena *arena, Theme *theme)`
   constructs the full `RenderTree` hierarchy from the canvas items, with
   resolved coordinates and styles
3. **FIL scripts** — each connection-generated script as a `static const char*`
4. **Widget data struct** — typedef with `WidgetBase`, `Arena*`, `Theme*`,
   `Store*`, and any widget instance pointers
5. **Render function** — calls `build_layout()`, resolves styles, copies to output
6. **Event handler** — dispatches events to appropriate FIL scripts or widget
   instances, manages focus/tab order, runs validation
7. **Factory function** — `Widget* my_plugin_new(void)` allocates and initializes
8. **Registration** — `void register_plugins(void (*reg)(const char*, WidgetFactory))`
   calls `reg("my_plugin", my_plugin_new)`

### Makefile template

```makefile
CC = gcc
CFLAGS = -std=c99 -Wall -O2 -fPIC -I$(FILLY_DIR)/src
LDFLAGS = -shared -lsodium -lm
OBJ = my_plugin.o

my_plugin.so: $(OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f *.o *.so
```

### FIL scripts in generated code

Each FIL script is stored as a string constant. The event handler calls
`fil_eval(script, store_get_safe, current_value)` to evaluate conditions
and execute `set`/`show`/`accept`/`reject` actions. The generated code
includes helper functions to:
- Map widget IDs to actual widget pointers
- Get/set widget values from FIL
- Show/hide widgets based on FIL `show` directives
- Route `accept`/`reject` to session-level responses

### Compile-and-load

After generation, `filly-build` can optionally invoke `make` in the output
directory and attempt to load the resulting `.so` into the running FILLY
instance for immediate testing (requires daemon mode with plugin hot-reload).

---

## Validation Pipeline (`validator.h`)

The validation pipeline runs on save and before export. It produces a
`ValidationReport` with issues sorted by severity.

### Checks

| Check                    | Severity | Description |
|--------------------------|----------|-------------|
| `check_duplicate_ids`    | Error    | Two items share the same `id` |
| `check_duplicate_names`  | Warning  | Two items share the same `instance_name` |
| `check_overlap`          | Warning  | Sibling items occupy overlapping space |
| `check_bounds`           | Error    | Any item rect extends beyond root bounds |
| `check_required_children`| Error    | Tabs/split_panes without children |
| `check_widget_params`    | Error    | Run `schema_validate()` on each item's params against widget schema |
| `check_keyboard_access`  | Error    | Interactive widgets missing keybinding |
| `check_tab_order`        | Warning  | Gaps or duplicates in tab_index |
| `check_color_contrast`   | Warning  | Foreground/background pairs below WCAG AA |
| `check_fil_syntax`       | Error    | FIL scripts fail to parse |
| `check_dangling_edges`   | Warning  | Ports with no connections |
| `check_type_mismatch`    | Error    | Edge connects incompatible types |
| `check_cycles`           | Error    | Circular connection path detected |
| `check_tui_compliance`   | Info     | Issues only visible in TUI mode |
| `check_missing_labels`   | Warning  | Interactive widgets without `accessible.label` |
| `check_store_vars_used`  | Info     | Store variables declared but never read/written |
| `check_orphan_widgets`   | Info     | Widgets with no connections and not decorative |

### Validation API

```c
typedef enum { V_ERROR, V_WARNING, V_INFO } ValidationSeverity;

typedef struct {
    ValidationSeverity severity;
    char *message;
    int item_id;          // -1 = project-level
    int node_id;          // -1 = not graph-related
    int edge_idx;         // -1 = not edge-related
    int keymap_idx;       // -1 = not keymap-related
} ValidationIssue;

typedef struct {
    ValidationIssue *issues;
    int count;
} ValidationReport;

ValidationReport *validator_check_all(BuilderProject *p);
void validation_report_free(ValidationReport *r);
```

The builder displays issues in a dockable panel at the bottom. Clicking an
issue navigates to the relevant item/node/edge. Export is blocked if any
errors exist.

---

## Integration with Existing Code

### Changes to `src/core/widget.c`

Add public enumeration function:

```c
bool widget_registry_enum(int *idx, const char **name, WidgetFactory *factory);
int widget_registry_count(void);
```

### Changes to `src/core/widget_factories.c`

Add public parameter descriptor accessor:

```c
const ParamDesc *widget_get_params(const char *widget_type, int *count);
```

Implementation: a static lookup table or if-else chain mapping each widget
name to its `ParamDesc` array and count. This is the same data used internally
by the factory functions.

### Changes to `src/core/widget_base.h`

No changes required. The builder creates widget instances using existing
`widget_registry_create()`.

### Changes to `src/core/render.h`

No changes required. All new data structures are in `src/builder/`.

### Changes to `Makefile`

Add `filly-build` target. Add `src/builder/` to include path (`-Isrc/builder`).

```makefile
BUILD_SRCS = src/builder/filly_build.c \
             src/builder/project.c \
             src/builder/canvas.c \
             src/builder/connection_graph.c \
             src/builder/property_editor.c \
             src/builder/codegen.c \
             src/builder/validator.c

filly-build: $(BUILD_SRCS) $(SRCS) src/cJSON.o
	$(CC) $(CFLAGS) $(GCORE_CFLAGS) -o $@ $^ $(LDFLAGS) $(GCORE_LDFLAGS)
```

---

## Implementation Phases

### Phase 1: Data model and project I/O
- `project.h` / `project.c` — all structs, `project_new`, `project_save`,
  `project_load`, item/node/edge manipulation, undo integration
- Add `widget_registry_enum` and `widget_get_params` to core
- Unit tests for project save/load round-trip

### Phase 2: Shell and canvas
- `filly_build.c` — `main()`, root widget, pane layout, mode switching
- `canvas.c` — live preview via headless pixel backend, zoom, pan, grid
- Palette populated from registry

### Phase 3: Property editor
- `property_editor.c` — dynamic form from `ParamDesc`, two-way binding,
  undo for property changes

### Phase 4: Connection graph
- `connection_graph.c` — node/port/edge rendering, wire drawing interaction,
  port compatibility checking, auto-node creation
- FIL generation from graph edges

### Phase 5: Code generation
- `codegen.c` — C code template engine, Makefile generation, FIL script
  embedding, compile-and-load integration

### Phase 6: Validation pipeline
- `validator.c` — all check functions, validation report, UI integration

### Phase 7: TUI compliance
- Tab order overlay, keybinding editor, TUI preview mode, TUI-specific
  validation checks

### Phase 8: Polish
- Alignment guides, multi-select, copy/paste, minimap, theming the builder
  itself, performance optimization (incremental preview updates)

---

## Testing Strategy

### Unit tests (`test/unit/test_builder.c`)
- Project save/load round-trip with all item types
- Port compatibility matrix
- FIL generation from simple and complex graphs
- C code generation output matches expected template
- Validation checks detect known issues
- Undo/redo for all operations

### Integration tests (`test/builder_test.sh`)
- Launch `filly-build --headless --project test_project.filly-project --export /tmp`
- Verify output directory structure
- Compile generated plugin, verify it loads
- Run generated plugin through `filly oneshot --headless` and check response

### Snapshot tests
- Render builder shell in headless pixel mode, compare to reference PNG
- Render built project in TUI preview mode, compare ANSI output

### Fault injection
- Corrupted project file → graceful error
- Missing widget type in registry → error in palette, not crash
- Circular connections → cycle detection prevents infinite loops
- Invalid FIL in edge transform → validation catches, doesn't crash codegen

---

## Future Enhancements (v0.10+)

- **Drag-and-drop from external files** — drop a JSON file on the canvas to
  import a widget definition
- **Live collaboration** — multiple builder instances connected to same daemon
  session, seeing each other's changes
- **Widget template library** — save/load individual widget configurations as
  reusable templates
- **Animation timeline** — keyframe editor for transition/animation properties
- **Accessibility simulator** — simulate screen reader output for the composed UI
- **Version control integration** — diff `.filly-project` files, merge changes
- **Plugin marketplace** — share builder-generated plugins via a central repository