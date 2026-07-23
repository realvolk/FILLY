CC = gcc
CFLAGS = -std=c99 -D_GNU_SOURCE -D_DEFAULT_SOURCE -Wall -Wextra -O2 -fPIC -Isrc
PKG_DEPS = libsodium
CFLAGS += $(shell pkg-config --cflags $(PKG_DEPS) 2>/dev/null)
LDFLAGS = -ldl -lpthread -lm -lsodium -lcrypt -rdynamic $(shell pkg-config --libs $(PKG_DEPS) 2>/dev/null)

GCORE_DEPS = libdrm libinput wayland-client xkbcommon x11 libudev gbm egl
GCORE_CFLAGS = $(shell pkg-config --cflags $(GCORE_DEPS) 2>/dev/null)
GCORE_LDFLAGS = $(shell pkg-config --libs $(GCORE_DEPS) 2>/dev/null) -lGLESv2 -lseccomp

PREFIX = /usr/local

SRCS = src/cli/main.c \
       src/protocol/protocol.c \
       src/protocol/schema.c \
       src/protocol/msgpack.c \
       src/core/render.c \
       src/core/widget.c \
       src/core/widget_base.c \
       src/core/session.c \
       src/core/store.c \
       src/core/theme.c \
       src/core/arena.c \
       src/core/clipboard.c \
       src/core/undo.c \
       src/core/relay.c \
       src/core/client.c \
       src/core/log.c \
       src/core/i18n.c \
       src/core/crypto.c \
       src/core/config.c \
       src/backend/terminal/terminal.c \
       src/backend/terminal/renderer.c \
       src/backend/headless/headless.c \
       src/backend/gcore/backend.c \
       src/backend/gcore/renderer.c \
       src/backend/gcore/wayland.c \
       src/backend/gcore/wayland-clipboard.c \
       src/backend/gcore/gpu.c \
       src/backend/daemon/daemon.c \
       src/backend/daemon/checkpoint.c \
       src/backend/daemon/verify.c \
       src/backend/daemon/sandbox.c \
       src/script/fil.c \
       src/core/widgets/menu.c \
       src/core/widgets/yesno.c \
       src/core/widgets/input.c \
       src/core/widgets/password.c \
       src/core/widgets/checklist.c \
       src/core/widgets/msg.c \
       src/core/widgets/filter.c \
       src/core/widgets/multiselect.c \
       src/core/widgets/file_picker.c \
       src/core/widgets/text_editor.c \
       src/core/widgets/summary.c \
       src/core/widgets/progress.c \
       src/core/widgets/toggle.c \
       src/core/widgets/spinner.c \
       src/core/widgets/separator.c \
       src/core/widgets/disk.c \
       src/core/widgets/table.c \
       src/core/widgets/tree.c \
       src/core/widgets/gauge.c \
       src/core/widgets/calendar.c \
       src/core/widgets/form.c \
       src/core/widgets/tabs.c \
       src/core/widgets/split_panes.c \
       src/core/widgets/context_menu.c \
       src/core/widgets/notification.c \
       src/core/widgets/radio_group.c \
       src/core/widgets/range_slider.c \
       src/core/widgets/color_picker.c \
       src/core/widgets/badge.c \
       src/core/widgets/rich_text.c \
       src/core/widgets/tooltip.c \
       src/core/widgets/hub.c

GCORE_SRCS = src/xdg-shell-protocol.c src/xdg-decoration-protocol.c

PLUGIN_ARTIXFORGE_SRCS = plugins/artixforge/plugin.c \
                          plugins/artixforge/install_hub.c \
                          plugins/artixforge/anvil.c \
                          plugins/artixforge/poweruser.c \
                          plugins/artixforge/recovery.c \
                          plugins/artixforge/iso.c \
                          plugins/artixforge/migration_init.c \
                          plugins/artixforge/migration_desktop.c \
                          plugins/artixforge/password_confirm.c \
                          plugins/artixforge/user_manager.c

PLUGIN_GFORGE_SRCS = plugins/gforge/plugin.c \
                      plugins/gforge/gforge_hub.c \
                      plugins/gforge/stage3_picker.c \
                      plugins/gforge/profile_picker.c \
                      plugins/gforge/kernel_picker.c \
                      plugins/gforge/use_flags.c \
                      plugins/gforge/cflags.c

TEST_SRCS = $(filter-out src/cli/main.c, $(SRCS)) $(GCORE_SRCS)

all: filly plugins

filly: $(SRCS) $(GCORE_SRCS) src/cJSON.o
	$(CC) $(CFLAGS) $(GCORE_CFLAGS) -DFILLY_GCORE -o $@ $^ $(LDFLAGS) $(GCORE_LDFLAGS)

src/cJSON.o: src/cJSON.c src/cJSON.h
	$(CC) $(CFLAGS) -c -o $@ $<

plugins: libartixforge.so libgforge.so

libartixforge.so: $(PLUGIN_ARTIXFORGE_SRCS)
	$(CC) $(CFLAGS) -Wno-misleading-indentation -shared -o $@ $^

libgforge.so: $(PLUGIN_GFORGE_SRCS)
	$(CC) $(CFLAGS) -shared -o $@ $^

filly-test: test/test.c $(TEST_SRCS) src/cJSON.o
	$(CC) $(CFLAGS) $(GCORE_CFLAGS) -DFILLY_GCORE -o $@ $^ $(LDFLAGS) $(GCORE_LDFLAGS)

test: filly-test
	./filly-test

pixel-test: test/pixel_test.c src/backend/gcore/renderer.c src/core/render.c src/core/theme.c src/core/arena.c src/cJSON.o
	$(CC) $(CFLAGS) $(GCORE_CFLAGS) -o pixel-test $^ $(LDFLAGS) $(GCORE_LDFLAGS)

install: all
	install -Dm755 filly $(PREFIX)/bin/filly
	mkdir -p $(HOME)/.config/filly/plugins
	cp libartixforge.so $(HOME)/.config/filly/plugins/
	cp libgforge.so $(HOME)/.config/filly/plugins/

tools: tools/genkey tools/sign tools/verify

tools/genkey: tools/genkey.c
	$(CC) $(CFLAGS) -o $@ $< -lsodium

tools/sign: tools/sign.c
	$(CC) $(CFLAGS) -o $@ $< -lsodium

tools/verify: tools/verify.c
	$(CC) $(CFLAGS) -o $@ $< -lsodium

snapshot: test/snapshot.c src/backend/gcore/renderer.c src/core/render.c src/core/theme.c src/core/arena.c src/cJSON.o
	$(CC) $(CFLAGS) $(GCORE_CFLAGS) -o $@ $^ $(LDFLAGS) $(GCORE_LDFLAGS)

artixforge-hub: plugins/artixforge/artixforge-hub.c src/core/client.c src/cJSON.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

test-unit: test/unit/test_arena.c src/core/arena.c \
           test/unit/test_theme.c src/core/theme.c src/core/render.c \
           test/unit/test_session.c src/core/session.c src/core/widget_base.c src/core/widget.c src/core/render.c src/core/store.c src/core/clipboard.c src/core/undo.c src/core/log.c src/backend/headless/headless.c src/backend/terminal/renderer.c src/protocol/protocol.c \
           test/unit/test_render.c \
           src/cJSON.o
	$(CC) $(CFLAGS) -o test-unit-arena test/unit/test_arena.c src/core/arena.c $(LDFLAGS)
	./test-unit-arena
	$(CC) $(CFLAGS) -o test-unit-theme test/unit/test_theme.c src/core/theme.c src/core/render.c src/cJSON.o $(LDFLAGS) -lsodium
	./test-unit-theme
	$(CC) $(CFLAGS) -o test-unit-session test/unit/test_session.c src/core/session.c src/core/widget_base.c src/core/widget.c src/core/render.c src/core/arena.c src/core/theme.c src/core/store.c src/core/clipboard.c src/core/undo.c src/core/log.c src/backend/headless/headless.c src/backend/terminal/renderer.c src/protocol/protocol.c src/cJSON.o $(LDFLAGS) -lsodium
	./test-unit-session
	$(CC) $(CFLAGS) -o test-unit-render test/unit/test_render.c src/core/render.c src/core/theme.c src/core/arena.c src/cJSON.o $(LDFLAGS) -lsodium
	./test-unit-render

test-fuzz: test/fuzz/protocol_fuzz.c src/protocol/protocol.c src/protocol/schema.c src/cJSON.o
	$(CC) $(CFLAGS) -fsanitize=fuzzer -o protocol_fuzz $^ $(LDFLAGS)
	./protocol_fuzz -max_len=4096

test-tui: filly
	./test/harness_tui.sh

test-gui: filly pixel-test
	./test/harness_gui.sh

test-all: test test-tui test-gui
	@echo "All test suites complete"

clean:
	rm -f filly filly-gui src/cJSON.o libartixforge.so libgforge.so filly-test artixforge-hub snapshot test-unit-arena test-unit-theme test-unit-session test-unit-render protocol_fuzz

.PHONY: all plugins install clean test test-unit test-fuzz tools