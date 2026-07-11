# Changelog

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