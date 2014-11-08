CC = clang
CFLAGS := -std=c11 -D_GNU_SOURCE -O2 -flto -fuse-ld=gold $(CFLAGS)
LDFLAGS := -O2 -flto -fuse-ld=gold -Wl,--as-needed,--gc-sections $(LDFLAGS)

ifeq ($(CC), clang)
	CFLAGS += -Weverything -Wno-cast-align -Wno-disabled-macro-expansion
else
	CFLAGS += -Wall -Wextra
endif

all: apply-pax-flags paxd-libre
apply-pax-flags: apply-pax-flags.o flags.o
paxd-libre: paxd-libre.o flags.o
flags: flags.c flags.h

clean:
	rm -f apply-pax-flags apply-pax-flags.o paxd-libre paxd-libre.o flags.o

install: paxd-libre paxd-libre.conf
	install -Dm755 apply-pax-flags $(DESTDIR)/usr/bin/apply-pax-flags
	install -Dm755 paxd-libre $(DESTDIR)/usr/bin/paxd-libre
	install -Dm600 paxd-libre.conf $(DESTDIR)/etc/paxd-libre.conf
	install -Dm644 paxd-libre.service $(DESTDIR)/usr/lib/systemd/system/paxd-libre.service
	mkdir -p $(DESTDIR)/usr/lib/systemd/system/sysinit.target.wants
	cd $(DESTDIR)/usr/lib/systemd/system/sysinit.target.wants; ln -sf ../paxd-libre.service .

.PHONY: all clean install
