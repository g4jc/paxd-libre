CC = clang
CFLAGS := -std=c11 -D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE \
	  $(shell pkg-config --cflags glib-2.0) \
	  -O2 -flto -fuse-ld=gold $(CFLAGS)
LDFLAGS := -O2 -flto -fuse-ld=gold -Wl,--as-needed,--gc-sections $(LDFLAGS)
LDLIBS := $(shell pkg-config --libs glib-2.0)

ifeq ($(CC), clang)
	CFLAGS += -Weverything -Wno-cast-align -Wno-disabled-macro-expansion -Wno-documentation \
		  -Wno-padded -Wno-reserved-id-macro
else
	CFLAGS += -Wall -Wextra
endif

all: paxd-libre
paxd-libre: paxd-libre.o flags.o
flags: flags.c flags.h

clean:
	rm -f paxd-libre paxd-libre.o flags.o

install: paxd-libre paxd-libre.conf
	install -Dm755 paxd-libre $(DESTDIR)/usr/bin/paxd-libre
	install -Dm600 paxd-libre.conf $(DESTDIR)/etc/paxd-libre.conf
	install -Dm644 paxd-libre.service $(DESTDIR)/usr/lib/systemd/system/paxd-libre.service
	install -Dm644 user.service $(DESTDIR)/usr/lib/systemd/user/paxd-libre.service
	mkdir -p $(DESTDIR)/usr/lib/systemd/system/sysinit.target.wants
	ln -t $(DESTDIR)/usr/lib/systemd/system/sysinit.target.wants -sf ../paxd-libre.service
	ln -sf paxd-libre.conf $(DESTDIR)/etc/paxd.conf

.PHONY: all clean install
