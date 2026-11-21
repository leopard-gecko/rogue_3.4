# Rogue 3.4 macOS/Unix compatibility port (stage 28)
CC ?= cc
CFLAGS ?= -O2
# Keep C11 and warning checks even when callers override CFLAGS (e.g., -O0 -g).
PORT_CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -Wmissing-prototypes
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
CPPFLAGS += -D_DARWIN_C_SOURCE
LDLIBS += -lcurses
else
CPPFLAGS += -D_DEFAULT_SOURCE
LDLIBS += -lncurses -lcrypt
endif

SRCS = vers.c main.c rooms.c passages.c command.c move.c fight.c chase.c \
       init.c things.c potions.c wizard.c list.c io.c rip.c daemon.c daemons.c \
       weapons.c monsters.c save.c options.c armor.c globals.c compat.c
OBJS = $(SRCS:.c=.o)

.PHONY: all clean
all: rogue
rogue: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)
%.o: %.c rogue.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(PORT_CFLAGS) -c -o $@ $<
clean:
	rm -f $(OBJS) rogue
