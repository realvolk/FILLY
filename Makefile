CC = gcc
CFLAGS = -std=c99 -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE -Wall -Wextra -O2 -fPIC -I.
LDFLAGS = -ldl -lpthread -lm -lsodium -rdynamic
PREFIX = /usr/local

SRCS = filly-bin/main.c \
       filly-protocol/protocol.c \
       filly-protocol/schema.c \
       filly-core/render.c \
       filly-core/widget.c \
       filly-core/session.c \
       filly-core/store.c \
       filly-core/theme.c \
       filly-core/relay.c \
       filly-terminal/terminal.c \
       filly-terminal/renderer.c \
       filly-daemon/daemon.c \
       filly-daemon/checkpoint.c \
       filly-daemon/verify.c \
       filly-headless/headless.c \
       filly-script/fil.c \
       filly-core/widgets/menu.c \
       filly-core/widgets/yesno.c \
       filly-core/widgets/input.c \
       filly-core/widgets/password.c \
       filly-core/widgets/checklist.c \
       filly-core/widgets/msg.c \
       filly-core/widgets/filter.c \
       filly-core/widgets/multiselect.c \
       filly-core/widgets/file_picker.c \
       filly-core/widgets/text_editor.c \
       filly-core/widgets/summary.c \
       filly-core/widgets/progress.c \
       filly-core/widgets/toggle.c \
       filly-core/widgets/spinner.c \
       filly-core/widgets/separator.c \
       filly-core/widgets/disk.c \
       filly-core/widgets/table.c \
       filly-core/widgets/tree.c \
       filly-core/widgets/gauge.c \
       filly-core/widgets/calendar.c \
       filly-core/widgets/form.c \
       filly-core/widgets/tabs.c \
       filly-core/widgets/split_panes.c \
       filly-core/widgets/context_menu.c \
       filly-core/widgets/notification.c \
       filly-core/widgets/radio_group.c \
       filly-core/widgets/range_slider.c \
       filly-core/widgets/color_picker.c \
       filly-core/widgets/badge.c \
       filly-core/widgets/rich_text.c \
       filly-core/widgets/tooltip.c \
       filly-core/widgets/hub.c

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

TEST_SRCS = $(filter-out filly-bin/main.c, $(SRCS))

all: filly plugins

filly: $(SRCS) cJSON.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

cJSON.o: cJSON.c cJSON.h
	$(CC) $(CFLAGS) -c -o $@ $<

plugins: libartixforge.so libgforge.so

libartixforge.so: $(PLUGIN_ARTIXFORGE_SRCS)
	$(CC) $(CFLAGS) -shared -o $@ $^

libgforge.so: $(PLUGIN_GFORGE_SRCS)
	$(CC) $(CFLAGS) -shared -o $@ $^

filly-test: test/test.c $(TEST_SRCS) cJSON.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

test: filly-test
	./filly-test

install: all
	install -Dm755 filly $(PREFIX)/bin/filly
	mkdir -p $(HOME)/.config/filly/plugins
	cp libartixforge.so $(HOME)/.config/filly/plugins/
	cp libgforge.so $(HOME)/.config/filly/plugins/

tools: tools/genkey tools/sign

tools/genkey: tools/genkey.c
	$(CC) $(CFLAGS) -o $@ $< -lsodium

tools/sign: tools/sign.c
	$(CC) $(CFLAGS) -o $@ $< -lsodium

clean:
	rm -f filly cJSON.o libartixforge.so libgforge.so filly-test

.PHONY: all plugins install clean test tools