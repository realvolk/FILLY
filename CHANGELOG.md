# Changelog


## (2026-08-03) — FILLY

### Housekeeping
- Added ForgeLFS plugin pack (`forge_hub`, `forge_anvil`) for LFS-style system construction
- Everything else I did not document

## v0.5.1 (2026-08-02) — FILLY

### Changed
- **Widget render API rewritten** — all 36 built-in widgets, 9 ArtixForge plugins, and 7 GForge plugins updated to new two-argument `render(Widget*, RenderTree*)` signature; `Rect area` now derived from `WidgetBase *base = (WidgetBase *)(self + 1); Rect area = base->render_area` at the top of each render function
- **RenderTree union naming** — all field accesses migrated to `node->u.field` naming convention: `u.text`, `u.list`, `u.input`, `u.container`, `u.badge`, `u.tabs`, `u.split_panes`, `u.table`, `u.tree`, `u.context_menu`, `u.form`, `u.gauge`, `u.calendar`, `u.spinner`, `u.toast`, `u.checkbox`, `u.toggle`, `u.separator`, `u.cursor`
- **Backend layout pass** — `layout_tree` updated with union field naming; all node type cases in layout switch use `child->u.*` accessors
- **Gcore backend** — `free_tree_copy`, `copy_tree_malloc`, `hit_test`, and `synthesize_key_events` updated to use `u.container` and `u.list` union fields
- **Gcore renderer** — `update_hover_states` and `render_node` updated with union field naming throughout all render node type cases
- **Plugin build system** — `-fPIC` added to plugin `.so` compilation flags; `-Wl,--unresolved-symbols=ignore-all` linker flag added to defer core symbol resolution to daemon `dlopen` time
- **Daemon plugin loading** — `dlopen` flags changed from `RTLD_NOW` to `RTLD_NOW | RTLD_GLOBAL` so plugins can resolve core symbols (`g_session_arena`, `arena_alloc`, `arena_strdup`) from the daemon binary at load time

### Fixed
- **ArtixForge plugins** — all 9 plugins (`install_hub`, `anvil`, `poweruser`, `recovery`, `iso`, `migration_init`, `migration_desktop`, `password_confirm`, `user_manager`) updated: render signatures, union field naming, overlay sub-widget dispatch, `widget_base_init` argument types
- **GForge plugins** — all 7 plugins (`gforge_hub`, `stage3_picker`, `profile_picker`, `kernel_picker`, `use_flags`, `cflags`, `plugin`) updated: render signatures, union field naming, `widget_base_init` argument types
- **POSIX compliance** — `port_linux.h` undefs `_POSIX_C_SOURCE` before defining `_GNU_SOURCE` for `SO_PEERCRED`/`struct ucred`; `daemon.c` defines `_GNU_SOURCE` before all includes; `progress.c` defines `_XOPEN_SOURCE 500` for `realpath`; `wayland.c` migrated from `memfd_create` to `shm_open`+`ftruncate` POSIX-compliant shared memory path
- **Unused function warnings** — `term_write_diff` in `terminal.c` wrapped in `#if 0` block for future incremental rendering work; `glyph_cache_put` in `renderer.c` similarly preserved
- **Unused variable warning** — `Rect area` removed from `tabs_render` in `tabs.c`
- **`badge_render` signature** — updated to match new `void (*render)(Widget*, RenderTree*)` vtable type

### Housekeeping
- Rewritten all widgets to match the new upgrades
- Everything else I did not document in my haitus of programming and trying to fix FILLY for several days straight, I want to poke my eyes out.

## v0.5.0 (2026-07-26) — FILLY

### Added
- **MessagePack wire format** — fully activated via handshake negotiation; daemon responds in MessagePack when client requests it
- **Device profile detection** — daemon detects SSH, local TTY, Wayland, X11, or headless environment and adapts theme defaults (high-contrast on SSH)
- **Gamepad input** — libinput gamepad events mapped to keyboard: buttons to Enter/Esc/Tab/Space, analog sticks to arrow keys
- **Touch input** — touch-to-click synthesis via RenderTree hit-testing
- **Terminal emulator widget** — embeds a PTY with scrollback buffer (64KB), resize handling via TIOCSWINSZ, and `/` search filter
- **Widget builder widget** — visual palette for composing widget layouts; F1/F2/F3 mode switching, arrow navigation, S to save as JSON
- **Macro recorder widget** — UI for record/playback/save/load of session macros
- **Macro recording subsystem** — `src/core/recorder.c` captures frames (widget, params, response, ANSI snapshot) and events to `.filly-rec` JSON; replay mode with response comparison
- **Fault injection harness** — `test/fault_inject.c` tests corrupted JSON, truncated messages, unknown widgets, deeply nested JSON, arena exhaustion, null parameters, empty strings, massive choice lists
- **Performance benchmarks** — `test/benchmark.c` measures frame time, arena peak, response status for standard workloads; outputs CSV
- **Snapshot testing** — ANSI output mode in addition to pixel comparison; `--mode pixel|ansi` flag
- **CI pipeline** — GitHub Actions workflows for valgrind, snapshot, lint (clang-tidy), cppcheck, benchmark, fault injection, FreeBSD, OpenBSD
- **Language bindings** — Python (ctypes), Go (cgo), Node.js (napi) with `libfilly.so` shared library target
- **FreeBSD capsicum support** — capability rights limiting on daemon startup
- **OpenBSD pledge support** — `pledge("stdio unix proc")` after socket bind
- **Style engine colour arithmetic** — `lighten(color)`, `darken(color)`, `alpha(color)`, `mix(color1, color2, amount)` functions in theme variable resolution
- **Glyph cache** — 256-entry cache in pixel renderer avoids repeated glyph rasterization
- **CJK font support** — Noto Sans CJK in font fallback chain
- **FIL store bindings** — `input` widget passes active store to `fil_eval` for reactive validation against store state
- **Damage-region tracking** — `dirty_rect` parameter in pixel renderer for incremental updates
- **libfilly.so** — shared library target for language bindings
- **clang-tidy and cppcheck** — Makefile targets `lint` and `cppcheck`; `.clang-tidy` config
- **GUI builder** — `filly-build` standalone binary for visual widget composition:
  - **Project data model** — `BuilderProject` with items, nodes, edges, keymaps, store variables, TUI config, undo stack; JSON serialization to `.filly-project` format
  - **Canvas** — infinite zoom (0.1x-5.0x), pan, snap-to-grid, 8-point resize handles, rubber-band multi-select, right-click context menu (delete/lock/bring to front/send to back), live pixel preview via headless backend, tab order overlay, grid toggle
  - **Palette** — populated dynamically from widget registry with category grouping; 33 built-in types plus extensions bucket for plugins
  - **Connection graph editor** — node/port/edge visual diagram, wire drawing from output to input ports, port compatibility checking (TRIGGER→any, type matching), edge condition/transform editing, custom user-defined ports, store node creation
  - **Property editor** — dynamic form generation from `widget_get_params()` ParamDesc arrays, keyboard field editing and navigation
  - **Code generation** — FIL script generation from graph edges, complete C plugin source with layout builder, event handlers, factory, registration, and Makefile output
  - **Validation pipeline** — 13 checks: duplicate IDs, overlap, bounds, keyboard access, tab order, colour contrast, FIL syntax, dangling edges, type mismatches, cycles, missing labels, unused store vars, orphan widgets
  - **Builder shell** — five-pane layout using three-pane `split_panes` (palette, canvas, properties, connection graph, status), mode switching (Edit/Wire/Preview/TUI via F1-F4), keyboard shortcuts throughout
  - **Headless export** — `--export` flag for CI integration
- **Animation engine** — keyframe-based with full interpolation pipeline:
  - **6 easing functions** — linear, ease-in, ease-out, ease-in-out, bounce, elastic
  - **Animatable properties** — fg_color, bg_color, border_color, accent_color, border_width, border_radius, font_size, font_weight, opacity, shadow_offset, shadow_blur, shadow_color, bg_gradient_to, scale_x, scale_y, rotation, translate_x, translate_y
  - **Keyframe system** — multi-keyframe `AnimationDef` with per-segment easing, loop/repeat/auto_reverse, on_complete triggers (store key + FIL script)
  - **Animation registry** — global named animation definitions loaded from theme JSON `"animations"` section
  - **Playback API** — `animation_play`, `animation_stop`, `animation_pause`, `animation_resume`, `animation_play_custom`
  - **Per-frame update** — `animation_update(tree, now_ms)` called in session render loop before draw
  - **TUI animation subset** — opacity dimming, border colour cycling, character-level typewriter reveal; transform properties ignored
  - **GPU transform support** — scale, rotation, translation applied via model matrix in gcore backend
  - **Widget ports** — `animation_end`(TRIGGER) output and `play_animation`(STRING) input on every widget type
- **FIL animation statements** — `animate "widget_id" with "name"`, `stop animation on "widget_id"`, `pause animation on "widget_id"`, `resume animation on "widget_id"`; `FilResult.animation_names`/`animation_targets`/`animation_count` fields
- **Three-pane split_panes** — `split_position2` and `third` RenderTree pointer in `split_panes` union for three-column layouts
- **Widget public API extensions** — `widget_registry_enum`, `widget_registry_count`, `widget_get_params`, `ParamDesc`/`ParamType` public typedefs in `widget.h`
- **RenderTree animation state** — `active_animations` array and `animation_count` field on every node
- **WidgetStyle transform properties** — `scale_x`, `scale_y`, `rotation`, `translate_x`, `translate_y` with defaults in `widgetstyle_default()`

### Changed
- **cJSON replaced** — upgraded from minimal hand-rolled parser to full cJSON v1.7.19 with `cJSON_CreateStringArray` and correct null handling
- **Widget factory system** — rewritten from broken `generic_factory` cast to per-widget factory functions with proper parameter extraction and array counting
- **Daemon portability** — `#ifdef __linux__` blocks replaced with `FILLY_INOTIFY`/`FILLY_KQUEUE` guards; `struct ucred` replaced with `filly_ucred_t`
- **Oneshot fallback** — when no terminal is available, oneshot mode falls back to headless backend and writes response to stdout
- **Main.c refactored** — 33 individual factory wrappers with `count_str_array` helper; duplicate `shm_ipc_create`/`shm_ipc_map` removed; `session_run_multi` suppressed; `parse_key_name` extended with HOME/END/PAGEUP/PAGEDOWN/DELETE/INSERT
- **daemon.c** — `strncpy` replaced with `memcpy`+null; `i18n_init()` called; `checkpoint.c` buffer increased; `sandbox.c` unused params silenced; `widget_registry_register("text", ...)` alias added
- **Theme engine** — `resolve_var` made public and extended with function parsing; `theme_apply_fil_styles` wired; `parse_color` made public; animation loading from theme JSON via `animation_registry_load_from_theme`
- **Session** — `store_get_wrapper` added for FIL store access; `animation_update` called per frame before draw; `#include "core/animation.h"` added
- **Gcore backend** — transition interpolation extended to all animatable properties (shadows, gradients, transforms); animation transform applied in `render_node`
- **Gcore renderer** — scale/rotation/translation applied to `x, y, w, h` before widget rendering
- **Terminal renderer** — TUI animation subset: opacity dimming at <0.3, muted colours at 0.3-0.7
- **README** — updated feature count (36 widgets, 4 backends, animation engine, GUI builder), project structure with `src/builder/` and `src/core/animation.c`, architecture diagram
- **Spec** — upgraded to v0.9.0 with animation engine, GUI builder, three-pane splits, and updated roadmap
- **Makefile** — `src/core/animation.c` added to SRCS; `src/builder/` files in `filly-build` target; `-Isrc/builder` flag; `filly-build` added to clean target

### Fixed
- **Arena allocator** — `arena_alloc` now zeroes returned memory, preventing stale RenderTree data from causing segfaults
- **Daemon double-read** — `handle_client` no longer discards the first message when no handshake is sent; widget requests work on first message
- **cJSON_CreateNull** — type field properly set to `cJSON_NULL` instead of 0
- **Generic factory type mismatch** — `WidgetFactory` signature mismatch causing undefined behavior; replaced with per-widget wrappers
- **Menu/checklist/filter/radio/context menu** — count derived from array size instead of requiring separate `count` parameter
- **Form field count** — derived from fields array
- **Checklist min/max params** — renamed from `min_sel`/`max_sel` to `min`/`max` to match JSON keys
- **Parse key name** — added HOME, END, PAGEUP, PAGEDOWN, DELETE, INSERT
- **Text editor widget** — registered as both `"text_editor"` and `"text"` for backward compatibility
- **xdg-shell.h** — forward declarations for opaque structs; removed `configure_bounds`/`wm_capabilities` listener fields
- **Wayland listener** — removed v4+ fields for compatibility with older wayland-client
- **daemon.c** — `#include "core/theme.h"` and `#include "filly-port/port.h"` added
- **main.c** — `#include "core/i18n.h"` added
- **Checkpoint truncation** — `tmpfile` buffer increased to 2560 bytes
- **stb_truetype** — null-pointer guards on `xoff`/`yoff` output parameters in `stbtt_GetCodepointBitmap`; double `FindGlyphIndex` calls consolidated; `FlattenCurves` double-allocation bug fixed; null checks on malloc returns throughout rasterizer
- **gcore renderer** — `update_hover_states` invalid `free()` on string literals fixed via `state_owned` flag on `RenderTree`
- **Makefile** — `src/core/shm_ipc.c` added to SRCS; `-Isrc/filly-port` added to CFLAGS; `-lutil` added to LDFLAGS; `src/core/widgets/terminal_emulator.c`, `widget_builder.c`, `macro_recorder.c`, `src/core/recorder.c`, `src/core/animation.c` added; `test-fault` and `test-bench` targets added

### Housekeeping
- 119/119 behavioral tests passing
- 14/14 C test suite tests passing
- 20/20 GUI integration tests passing
- Zero compilation errors; warnings limited to intentional truncation and unused parameters
- 36 widgets: 33 original + terminal_emulator + widget_builder + macro_recorder
- 4 backends: terminal, gcore (DRM/X11/Wayland), headless, headless pixel
- Source tree additions: `src/core/animation.c`, `src/builder/` (7 files), `src/builder/build-spec.md`
- 87 unpushed commits

## v0.4.0 (2026-07-23) — FILLY

### Changed
- **Spec v0.5 parity achieved** — all 16 major sections of the formal specification are now implemented or scaffolded; core protocol, widget system, backends, plugin system, security model, and test suite fully match spec; remaining gaps are v0.6+ roadmap items (or will be done soon)
- **Source tree reorganized** — all source files moved under `src/` with subdirectories: `src/core/` (widget system, session, store, theme, arena, clipboard, undo, relay, client, config, i18n, crypto, log), `src/backend/` (terminal, headless, gcore, daemon), `src/cli/` (main), `src/protocol/` (protocol, schema, msgpack), `src/script/` (FIL), `src/themes/` (JSON theme files); vendored `cJSON.c`/`cJSON.h` and `stb_truetype.h` moved to `src/`
- **Terminal backend rewritten** — ANSI escape sequences now written directly to `/dev/tty` via `write_all` loop; alternate screen disabled due to kitty ESC byte race; terminal cleared with `\033[2J\033[H` on each draw frame; teardown restores cooked mode with `TCSADRAIN` then clears screen, resets attributes, and shows cursor; mouse support enabled via SGR extended coordinates on non-Linux-console terminals
- **Terminal renderer refactored** — `set_style` emits complete SGR sequences with single `m` terminator for foreground and background colors; `draw_text_wrapped` supports word-break wrapping and text alignment (left/center/right); `draw_list` highlights selected items with accent-color background and white foreground; `draw_box` supports single, double, and rounded borders with Unicode box-drawing characters; `fill_rect` clears container interiors; `draw_calendar` renders month grid with proper day-of-week alignment and selected-day highlighting; `render_tree_to_buf` begins with `\033[2J\033[H` and ends with `\033[0m` attribute reset; `RNODE_TEXT` now applies `text.align` from the render node to `draw_text_wrapped`
- **Relay rewritten** — `relay_setup` opens `/dev/tty`, sets raw mode, enters alternate screen, hides cursor, enables mouse; `relay_teardown` resets attributes, exits alternate screen, shows cursor, restores cooked mode; `parse_csi` handles all CSI/SS3 escape sequences for keyboard and mouse input; `draw_callback` writes raw ANSI frames directly to TTY
- **Session loop refactored** — `session_run` now polls terminal size every iteration and marks widget dirty on resize; first frame is drawn before entering event loop to ensure immediate rendering; `EVENT_RESIZE` and `EVENT_MOUSE_MOTION` mark widget dirty without dispatching to widget; clipboard paste (Ctrl+V), undo (Ctrl+Z), and redo (Ctrl+Y) handled at session level; store version polling triggers re-render on state changes
- **Menu widget** — box height now calculated from content (title lines + message lines + item count + footer) instead of using percentage of terminal height; box vertically centered; title, message, and footer text center-aligned within the box; children offset by +1 row to stay inside border; empty rows inside box eliminated
- **Message widget** — box height calculated from message line count; all text center-aligned; children offset inside border
- **Oneshot terminal output** — response JSON printed to stderr with leading `\r` for column-1 alignment; stdout left clean for shell
- **Makefile** — `TEST_SRCS` now includes `$(GCORE_SRCS)` so `filly-test` links Wayland protocol symbols; `src/cJSON.o` compiled as separate object

### Fixed
- **Terminal escape code leakage** — `\033[0m` attribute reset emitted before terminal teardown prevents SGR sequences from persisting after widget exit; `TCSADRAIN` used instead of `TCSAFLUSH` to avoid discarding output buffer
- **Title centering** — `draw_text_wrapped` now respects `text.align` field from render node, overridden via local `WidgetStyle` copy in `RNODE_TEXT` case
- **Box interior padding** — `fill_rect` clears container interior after border draw, preventing border characters from bleeding into centered content
- **Widget resize responsiveness** — `session_run` polls terminal size before dirty check, marks widget dirty on size change, enabling widgets to re-layout without user input
- **Kitty ESC byte race** — escape sequences written before `tcsetattr` switches to raw mode, preventing terminal from echoing partial sequences as text; alternate screen disabled to avoid screen-switch timing issues
- **Duplicate frame on startup** — first frame drawn before event loop to prevent blank screen flash
- **Relay alternate screen** — `relay_setup` now enters alternate screen and hides cursor, matching terminal backend behavior; `relay_teardown` exits alternate screen and restores cursor

### Housekeeping
- 119/119 headless behavioral tests passing
- 14/14 C test suite tests passing
- Zero warnings from `gcc -std=c99 -Wall -Wextra -O2` (except two intentional `strncpy`/`snprintf` truncation warnings in daemon and checkpoint)
- Source tree: `src/` with `core/`, `backend/`, `cli/`, `protocol/`, `script/`, `themes/`; `plugins/` with `artixforge/` and `gforge/`; `test/` with `unit/` and `fuzz/`; `tools/` with `genkey`, `sign`, `verify`

## v0.3.0 (2026-07-20) — FILLY

### Changed
- **Spec v0.4 parity** — all sections of the formal specification are now implemented, including session persistence, reactive store subscriptions, inactivity timeout, checkpoint recovery, Ed25519 plugin signing, FIL execution timeout, progress command allowlist, TTY ownership validation, and `--insecure-plugins` flag
- **Daemon: checkpoint persistence** — active sessions and their store state are serialized to `~/.cache/filly/checkpoint.json` (0600 permissions) every 10 connections; restored on daemon restart; sensitive keys (passwords, LUKS, tokens) are filtered from the checkpoint
- **Daemon: inactivity timeout** — clients that do not send messages within 30 seconds are disconnected with an error response; session resources are freed
- **Daemon: `--insecure-plugins` flag** — skips `.sig` file verification for plugin loading, allowing unsigned plugins during development; flag is scanned early so it applies to `oneshot` and `demo` as well as `daemon`
- **Daemon: Unix socket hardening** — default socket path moved to `$XDG_RUNTIME_DIR/filly.sock` with `0600` permissions; falls back to `/tmp/filly.sock` if XDG_RUNTIME_DIR is unset
- **Daemon: TTY ownership validation** — relay mode verifies that the target TTY is owned by the daemon's UID before opening it, preventing cross-user TTY hijacking
- **Daemon: stdio detachment** — daemon redirects stdin/stdout/stderr to `/dev/null` after binding the socket, preventing SIGTTOU when backgrounded
- **Store: `store_enum` iterator** — enables walking all key-value pairs for checkpoint serialization and debugging
- **Plugin loader: Ed25519 signature verification** — `libsodium` `crypto_sign_verify_detached` validates `.so.sig` files against an embedded public key; unsigned plugins are rejected unless `--insecure-plugins` is set; `tools/genkey` generates keypairs, `tools/sign` produces detached signatures
- **Headless backend: auto-EOF sentinel** — when the injected event queue drains, a single ESC event is synthesized, then the session exits gracefully after 5000 idle cycles; enables fully automated testing
- **Headless backend: event injection framework** — `filly oneshot --headless --events <file>` reads `KEY:<name>`, `TEXT:<string>`, and `WAIT:<ms>` directives; injects them into the headless event queue for deterministic widget testing
- **Progress widget: command allowlist** — `execvp` is restricted to binaries under `/usr/bin/`, `/usr/sbin/`, `/bin/`, `/sbin/` resolved via `realpath(3)`; attempts to execute outside these paths produce an error message instead of forking
- **FIL scripting: 1-second execution timeout** — `SIGALRM` with `sigsetjmp`/`siglongjmp` aborts script evaluation if it exceeds 1 second; timed-out scripts return `accepted: false` with an error message
- **Widget: tree** — ESC now returns a response with `cancelled: false`; ENTER and SPACE expand/collapse separated into distinct cases
- **Widget: text_editor** — cursor now defaults to end of content instead of position 0, so backspace and append behave correctly; BACKSPACE at beginning of line joins with previous line
- **Widget: progress** — `output` field initialized to empty string instead of NULL, preventing segfault on first render
- **Widget: separator, tooltip, rich_text** — now dismiss on any key event instead of returning `UNHANDLED` indefinitely, preventing headless session hangs
- **GForge plugins: null-safety** — `stage3_picker`, `profile_picker`, `kernel_picker` factories and event handlers guard against zero-length choice arrays and null `cJSON_GetObjectItem` returns, preventing segfaults on minimal JSON payloads
- **GForge: `gforge_hub` registered** — the main Gentoo configuration hub widget was compiled but not registered in `plugin.c`; fixed

### Added
- **`filly oneshot --headless`** — runs a single widget in headless mode; with `--events <file>` injects synthetic key events for automated testing
- **`filly test`** — validates that the binary and plugin loading are functional
- **Test harness: `test/harness.sh`** — 119 behavioral tests covering all 33 built-in widgets plus 9 ArtixForge and 6 GForge plugin widgets; exercises selection, input, validation, cancellation, boundary conditions, visibility conditions, multi-category hub editing, quick profiles, confirmation dialogs, password matching, user add/edit/delete, USE flag toggling, CFLAGS field navigation, and multi-step installer workflows
- **Headless event file format** — `KEY:<name>` for navigation/function keys, `KEY:SPACE` for space, `TEXT:<string>` for character sequences, `WAIT:<ms>` for delays
- **`filly_graphical.sh`: 9 missing wrappers** — `filly_graphical_separator`, `install_hub`, `recovery`, `iso`, `migration_init`, `migration_desktop`, `poweruser`, `password_confirm`, `user_manager`

### Fixed
- **Headless backend: name collision** — `h` parameter shadowed `HeadlessBackend *h` in `headless_backend_init` and `headless_inject_resize`; renamed height parameter
- **Store: missing includes** — `stdio.h` and `unistd.h` added for `snprintf` and `write`
- **Daemon: `use_msgpack` warning** — variable is now referenced in handshake response
- **Main: `set_insecure_plugins` declaration** — extern added for `--insecure-plugins` flag
- **Main: `usleep` portability** — replaced with `poll(NULL, 0, ms)` for millisecond waits
- **Main: early `--insecure-plugins` scan** — flag is parsed before `load_plugins()` so that `oneshot` and `demo` modes can load unsigned plugins
- **FIL: missing `unistd.h`** — added for `alarm()` declaration
- **FIL: misleading indentation** — `if`/`for` bodies properly braced to eliminate `-Wmisleading-indentation` warnings
- **Progress: missing `limits.h`** — added for `PATH_MAX` and `realpath` declaration
- **Widget: form, table, tabs, context_menu, hub, disk** — braced `if`/`for` bodies, added fallthrough comments, fixed misleading indentation
- **Widget: color_picker** — ANSI 256-color escape codes use proper format
- **GForge: `cflags` widget factory** — corrected registration to use proper factory function
- **GForge: `gforge_hub` event handling** — fixed category/item navigation bounds checking
- **Checkpoint: format-truncation warning** — suppressed with `-D_DEFAULT_SOURCE`
- **Daemon: `strncpy` truncation warning** — suppressed with `-D_DEFAULT_SOURCE`

### Housekeeping
- Zero warnings with `gcc -std=c99 -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE -Wall -Wextra -O2`
- `make clean && make` produces `filly` binary and both plugin `.so` files
- `make tools` produces `tools/genkey` and `tools/sign`
- 119/119 headless tests passing: 33 built-in, 16 ArtixForge, 14 GForge, plus comprehensive behavioral coverage
- Python graphical backend: `base.py` uses singleton `Adw.Application`, Escape key controller on all windows, `hub.py` implements full `user_manager` and `password_confirm` sub-widget dialogs, `progress.py` includes stage detection matching C implementation, `disk.py` implements full partition editing operations

## v0.2.3 (2026-07-17) — FILLY

### Changed
- **Relay draws to `/dev/tty`** — DRAW packets write directly to the TTY file descriptor instead of stdout or stderr, preventing ANSI escape sequences from being captured by shell pipelines
- **Relay sends terminal size before request** — `SIZE` message with actual terminal dimensions sent before the JSON payload, so the daemon renders at the correct size on the first frame
- **Relay handles Linux console F-keys** — `\033[[A` through `\033[[E` sequences mapped to F1-F5 for physical TTYs and virtual consoles

### Fixed
- **Hub sub-widget dispatch** — unknown widget types (`user_manager`, `disk`, custom plugins) now fall through to a generic sub-widget path using the full widget registry, instead of being silently ignored
- **Hub sub-widget result handling** — JSON objects and arrays from sub-widgets are serialized and stored correctly in hub state
- **Daemon client tracking** — connection acceptance, widget creation, and session lifecycle events logged for debugging
- **Newline-delimited protocol enforcement** — hub JSON compacted with `jq -c .` before relay transmission, preventing multi-line payloads from being split into multiple daemon requests

## v0.2.2 (2026-07-16) — FILLY

### Changed
- **Relay key protocol rewritten** — `relay.c` now implements the same CSI/SS3 escape sequence parser as `terminal.c`, sending `KeyCode` enum values instead of raw bytes. This fixes the daemon-relay path for all interactive widgets (menus, inputs, hub, etc.) that previously received unmapped key codes
- **Install hub profile system refactored** — `hub_apply_profile` replaced 200+ lines of repetitive if-else chains with a data-driven `ProfileDef` table using designated initializers. Each profile is a single row specifying only the fields it overrides
- **POSIX compliance pass** — build now uses `-std=c99 -D_POSIX_C_SOURCE=200809L`; all GCC `?:` extensions replaced with explicit ternary expressions; `strcasecmp` replaced with standard C `str_case_eq` helper

### Fixed
- **Daemon-relay interactive path** — relay now parses escape sequences from `/dev/tty` and sends `KeyCode` enum integers to the daemon, matching the socket backend's `KEY %d %c` protocol. Previously raw bytes were sent, producing codes that fell through all widget switch cases
- **Session NULL guard** — `session_run` validates widget, backend, and vtable function pointers before calling, preventing segfaults from incomplete plugin widgets
- **Widget registry NULL guard** — `widget_registry_create` checks factory pointer before calling
- **Install hub struct initializer warnings** — `ProfileDef` table uses designated initializers, eliminating excess element and missing initializer warnings across all 18 profiles
- **yesno widget fallthrough** — fixed implicit fallthrough warning in `KEY_CHAR` handler for `'n'`/`'N'` quick-select

### Removed
- **Dead socket backend** — `filly-daemon/socket_backend.c` and `socket_backend.h` removed; the socket vtable is defined directly in `daemon.c` and the separate file was never compiled or linked

### Housekeeping
- All widget and plugin `handle_event` functions now explicitly cast unused `backend` parameter to `(void)`, yielding a clean `-Wall -Wextra` build with zero warnings
- Removed unused functions: `cmp_entries` (file_picker.c), `first_weekday` (calendar.c), `flatten` (tree.c)
- Retained but silenced with `(void)` casts: partition type/flag arrays in `disk.c` (reserved for TYPE_PICKER/FLAG_PICKER modes), group lists in `user_manager.c` (reserved for group selection UI)

## v0.2.1 (2026-07-15) — FILLY

### Changed
- **Daemon TTY passthrough** — daemon now accepts a `tty` field in widget requests, opening the client's terminal instead of `/dev/tty`; interactive widgets render on and read keystrokes from the correct terminal
- `terminal_backend_init` signature changed to accept `tty_path` parameter; callers pass `NULL` for default `/dev/tty` behavior
- `WidgetRequest` struct extended with `tty` field; parsed and freed alongside other fields
- `load_plugins` moved to `daemon.h` and made non-static for reuse by oneshot mode
- Plugin directory changed from `~/.config/filly/plugins/` to `$HOME/.config/filly/plugins/` — resolves correctly under `sudo` where `$HOME` is `/root`
- Binary linked with `-rdynamic` to export symbols for plugin resolution

### Added
- `choices_file` support in hub items — reads choice lists from `/tmp/artix-installer/filly-data/` files instead of embedding large JSON arrays
- Data file generation in installer `menu.sh` — kernels, extras, timezones, locales, keymaps written to disk before hub launch, eliminating JSON payload size limits
- Plugin load error logging — `dlerror()` output printed to stderr on `dlopen` or `dlsym` failure
- Daemon read buffer increased to 512KB for large hub JSON payloads
- Calendar widget month/year and day-of-week headers centered within the box width
- Split panes dismiss with ESC and switch active pane with F1/F2
- Text editor receives Home/End/PageUp/PageDown/Delete key support, Ctrl+S save, Ctrl+D delete line, visible inverse-video cursor block, and multiline content splitting
- Gauge bar uses safe ASCII `=`/`-` characters across all locales
- Color picker converts RGB to nearest ANSI 256-color palette entry

### Fixed
- Container interior cleared with `fill_rect` after border draw, preventing border character bleed-through into centered text
- Text lines padded to full width after drawing, eliminating trailing border artifacts
- `textstyle_selected` given a background color so title text covers the border line beneath it
- Terminal corruption on exit — alt screen and raw mode teardown made idempotent, called once per session
- F-key parsing — CSI sequences with multi-digit parameters correctly mapped to F1-F12
- ESC key detection — lone ESC distinguished from escape sequence prefixes via inter-byte timeout
- Daemon segfault — `Backend.data` initialized to `&t` instead of garbage stack memory
- Daemon plugin loading — `$HOME` resolution under `sudo` now finds `/root/.config/filly/plugins/`
- Daemon socket buffer overflow — increased to 512KB
- Form widget factory properly implemented with keyboard input, Tab/Shift+Tab navigation, Enter to submit
- Tabs, split panes, and tree widget factories implemented — previously returned NULL stubs
- Yes/No prompt in installer GUI detection — now calls `filly_yesno` directly, avoiding stale `FILLY_BACKEND` env var leaks
- `FILLY_BACKEND` explicitly set to `tui` before GUI prompt, preventing premature GUI function calls
- GUI backend validation — checks for `filly_graphical.sh` and `base.py` existence before enabling GUI mode
- Notification message vertically centered in toast box
- Badge and spinner widgets dismiss on keypress/ESC to allow demo progression
- Range slider bar width calculation fixed; percentage display added
- Color picker uses ANSI 256-color conversion instead of malformed hex escape sequences
- Toggle widget switch line centered
- `install_hub` communicates with daemon via `socat` for reliable bidirectional Unix socket transfer

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