CC = clang
CFLAGS := -std=c11 -O2 $(CFLAGS)
LDFLAGS := -Wl,--as-needed $(LDFLAGS)

ifeq ($(CC), clang)
	CFLAGS += -Weverything -Wno-cast-align -Wno-disabled-macro-expansion
else
	CFLAGS += -Wall -Wextra
endif

paxd-libre: paxd-libre.c

install: paxd-libre paxd-libre.conf
	install -Dm755 paxd-libre $(DESTDIR)/usr/bin/paxd-libre
	install -Dm600 paxd-libre.conf $(DESTDIR)/etc/paxd-libre.conf
	install -Dm644 paxd-libre.service $(DESTDIR)/usr/lib/systemd/system/paxd-libre.service
	mkdir -p $(DESTDIR)/usr/lib/systemd/system/sysinit.target.wants
	cd $(DESTDIR)/usr/lib/systemd/system/sysinit.target.wants; ln -sf ../paxd-libre.service .
