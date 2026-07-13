# Changelog

## v0.2.0 (2026-07-13) — FILLY

### Changed
- **Complete rewrite of core in C** — replaced Rust (`filly-core`, `filly-terminal`, `filly-daemon`, `filly-protocol`, `filly-bin`) with ANSI C using raw termios + ANSI escapes; Python GTK4 graphical backend unchanged
- Widget trait system ported to C vtable pattern — `WidgetVTable` struct with `render`, `handle_event`, `is_dirty`, `clear_dirty`, `destroy` function pointers
- RenderTree replaced with direct ANSI escape rendering — `renderer.c` walks the tree and writes escape sequences to a buffer with styled box borders (single/double/rounded), text wrapping with word break, scrollable lists with selected highlight, gauge bars with percentage overlay, calendar month grid with day selection, form field highlighting, tabbed content areas, split pane dividers, tree indentation with expand/collapse markers, context menu with highlight, toast notifications, and 256-color styled borders
- Plugin system uses `dlopen`/`dlsym` — `.so` files export `register_plugins(void (*reg)(const char *, WidgetFactory))`
- Build system: single `Makefile` producing `filly` binary + `libartixforge.so` + `libgforge.so`; `cJSON` vendored as single-file dependency
- JSON protocol unchanged — same `WidgetRequest`/`WidgetResponse` schema, same daemon socket behavior, same CLI interface (`daemon`, `oneshot`, `batch`, `demo`)
- `fil.sh` and `filly_graphical.sh` wrappers unchanged — same function signatures, same JSON protocol
- Rust toolchain no longer required — only `gcc`, `make`, `cJSON`
- Terminal input parser rewritten with `select()`-based timeout — handles multi-byte CSI sequences, SS3 F-keys, and lone ESC detection without blocking

### Added
- `Makefile` with `all`, `plugins`, `install`, `clean` targets
- `cJSON.c`/`cJSON.h` vendored at repository root
- All 33 widgets ported to C with identical behavior to v0.1.1
- ArtixForge plugin pack: `install_hub`, `anvil`, `poweruser`, `recovery`, `iso`, `migration_init`, `migration_desktop`, `password_confirm`, `user_manager`
- GForge plugin pack: `gforge_hub`, `stage3_picker`, `profile_picker`, `kernel_picker`, `use_flags`, `cflags`
- `install_hub` widget with full inline editing — menu, input, password, yes/no, filter, and multiselect sub-widgets rendered as centered modal overlays with independent event handling
- Text editor widget with Home/End/PageUp/PageDown/Delete, Ctrl+S save, Ctrl+D delete line, visible inverse-video cursor block, and multiline content loading
- Range slider with interactive bar, percentage display, and keyboard controls
- Color picker with RGB sliders, ANSI 256-color preview, hex output, and per-channel adjustment
- Split panes with F1/F2 pane switching and ESC to dismiss
- Tabs widget with left/right navigation and embedded child widgets
- Tree widget with expand/collapse and keyboard navigation
- Interactive calendar with month grid, day selection, arrow key navigation
- Gauge widget with percentage bar and label
- Toggle widget with centered ON/OFF switch
- Badge widget with dismiss-on-keypress
- Toast notification with centered message and auto-dismiss timer
- Session-aware terminal setup/teardown — alt screen and raw mode toggle only once per session, preventing repeated screen flicker across widget transitions

### Removed
- Rust workspace (`Cargo.toml`, all `src/` directories)
- Rust dependencies (`crossterm`, `ratatui`, `clap`, `serde`, `serde_json`, `libloading`, `dirs`, `uuid`, `anyhow`)
- Rust plugin `.so` files from `target/release/`

### Fixed
- Title border rendering — borders now draw correctly with proper corner characters, horizontal lines, and vertical sides spanning the full widget height
- Title text appears centered in the top border with a background fill that covers the border line beneath it
- Text centering across all widgets — messages, footers, and content blocks properly aligned with padding that fills the full line width to prevent border bleed-through
- Terminal corruption on exit — alt screen and raw mode teardown now idempotent, called once per session instead of once per widget
- F-key parsing — CSI sequences with multi-digit parameters (`\033[11~` through `\033[24~`) correctly mapped to F1-F12
- ESC key detection — lone ESC distinguished from escape sequence prefixes via inter-byte timeout
- Hub widget F1/ESC handling — confirmation prompts render as proper dialogs with Yes/No options
- Hub sub-widget dirty flag propagation — editing mode renders correctly on first frame
- Widget-in-hub overlay rendering — sub-widgets render as centered overlays with dark background on top of hub
- Disk picker populates dynamic `lsblk` list at runtime
- Kernel picker uses flat searchable filter list instead of nested sub-menus
- Yes/No widgets carry descriptive messages into edit overlay
- Password confirm widget uses safe hashing without shell injection
- User manager widget supports add/edit/delete with group selection and password confirmation
- ANSI escape sequence leak into JSON output — escape sequences stripped before parsing
- Terminal cursor restoration after widget exit — newline printed after JSON response
- Form widget factory now properly implemented — fields accept keyboard input, Tab/Shift+Tab navigation, Enter to submit
- Tabs and split panes factory implementations — previously returned NULL stubs
- Tree widget factory implementation — previously returned NULL stub
- Gauge bar uses safe ASCII characters across all locales
- Color picker converts RGB to nearest ANSI 256-color palette entry, avoiding malformed escape sequences
- Notification message vertically centered in toast box
- Badge and spinner widgets dismiss on keypress/ESC to allow demo progression

## v0.1.1 (2026-07-11) — FILLY

### Added
- Plugin auto-loading in `filly oneshot` and `filly batch` modes (previously daemon-only)
- GUI plugin discovery system in `backend.py` — scans `plugins/*/python/` and `~/.config/filly/gui-plugins/` for `register()` hooks
- `GuiBackend.register_widget()` classmethod for external widget registration
- ArtixForge and GForge plugin crates added to workspace members
- Python GUI plugin packs: ArtixForge (6 hub widgets → `HubWindow`) and GForge (5 custom widgets + 11 portage widgets → built-in FILLY widgets)
- GForge GUI widgets: `stage3_picker`, `profile_picker`, `kernel_picker`, `use_flags`, `cflags`

### Changed
- `WidgetFactory` type changed from `fn` pointer to `Box<dyn Fn>` for closure support
- `PluginRegistry::register` now accepts generic `impl Fn` instead of requiring function pointers
- Daemon's `handle_client` uses immutable store binding to fix unused-mut warning

## v0.1.0 (2026-07-10) — FILLY

Initial release of the Forge Interface Linux Library.

### Added

**Core Architecture**
- Multi-backend widget library with single JSON protocol
- Terminal backend via crossterm + ratatui 0.29
- Graphical backend via GTK4 + libadwaita (Python)
- Unix socket daemon with session persistence and client multiplexing
- RenderTree abstraction decoupling widgets from rendering
- Plugin system with dynamic `.so` loading
- Headless backend for testing and CI validation

**33 Widget Types**
- Input widgets: input, password, form, text editor, file picker
- Selection widgets: menu, yesno, checklist, multiselect, filter, radio group, calendar, color picker, range slider
- Display widgets: message, summary, notification, badge, tooltip, rich text, spinner, separator, gauge
- Container widgets: hub, tabs, split panes, table, tree, context menu
- Advanced widgets: progress, disk, button, toggle

**Protocol**
- Newline-delimited JSON over Unix sockets and stdin/stdout
- Binary mode via MessagePack
- Streaming responses for progress widgets
- State subscriptions for reactive UIs
- Session management (create/destroy/persist)
- Theme reload control messages

**CLI**
- `filly demo` — run all widgets in sequence
- `filly oneshot` — process single JSON request
- `filly batch` — process NDJSON requests
- `filly daemon` — start Unix socket listener
- `filly validate` — validate JSON requests
- `filly schema` — print JSON Schema for widgets
- `filly new-widget` — scaffold new widget source files

**Client Libraries**
- Bash (`fil.sh`) — drop-in replacement for gum and forge-tui shell functions
- Python (`filly.Session`) — context manager with idiomatic API
- Go (`go-filly`) — Unix socket client library
- Node.js (`node-filly`) — npm package with async/await support

**Plugin Packs**
- ArtixForge — 7 domain widgets (anvil, install hub, power user, recovery, ISO builder, init migration, desktop migration)
- GForge — 16 Gentoo-specific widgets (stage3, profiles, USE flags, CFLAGS, portage configuration)

**Theming**
- 8 built-in themes: Forge, Monokai, Solarized Dark, Dracula, Nord, Gruvbox Dark, Catppuccin Mocha, Tokyo Night
- JSON stylesheet engine with inheritance
- Live theme reload via `reload_theme` control message
- `--theme` flag on daemon, oneshot, and demo commands

**Features**
- Incremental rendering via dirty flag system
- Clipboard paste from system clipboard (wl-paste, xclip)
- Clipboard copy from text editor
- OSC 8 hyperlink support in rich text widget
- Desktop notifications via `notify-send`
- Accessibility metadata on all GUI widgets via AT-SPI
- Accessibility fields in terminal render tree for future screen reader support
- Full TUI/GUI widget parity across all 33 widget types

**Testing & Tooling**
- Schema validation for all widget types
- Widget scaffolding command
- Headless backend for snapshot testing
- Clean build with `cargo build --release` — single static binary

### Changed
- Merged forge-tui and forge-gui into unified architecture
- Widgets use remaining-space layout with clamped dimensions to prevent buffer overflows