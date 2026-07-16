CC = gcc
CFLAGS = -std=c99 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -O2 -fPIC -I.
LDFLAGS = -ldl -lpthread -lm -rdynamic
PREFIX = /usr/local

SRCS = filly-bin/main.c \
       filly-bin/relay.c \
       filly-protocol/protocol.c \
       filly-core/render.c \
       filly-core/widget.c \
       filly-core/session.c \
       filly-core/store.c \
       filly-core/theme.c \
       filly-terminal/terminal.c \
       filly-terminal/renderer.c \
       filly-daemon/daemon.c \
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

install: all
	install -Dm755 filly $(PREFIX)/bin/filly
	mkdir -p $(HOME)/.config/filly/plugins
	cp libartixforge.so $(HOME)/.config/filly/plugins/
	cp libgforge.so $(HOME)/.config/filly/plugins/

clean:
	rm -f filly cJSON.o libartixforge.so libgforge.so

.PHONY: all plugins install clean