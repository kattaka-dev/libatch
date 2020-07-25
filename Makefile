.PHONY: all clean install uninstall

CC = gcc
#CC = clang

SRCDIR = src
OBJS = $(SRCDIR)/atchannel.o $(SRCDIR)/at_tok.o $(SRCDIR)/misc.o
HEADER = $(SRCDIR)/atchannel.h
LIBNAME = libatch
LIBVERSION_MAJOR = 0
LIBVERSION_MINOR = 0
LIBVERSION = $(LIBVERSION_MAJOR).$(LIBVERSION_MINOR)
BIN = $(LIBNAME).so.$(LIBVERSION)
BIN_MAJOR = $(LIBNAME).so.$(LIBVERSION_MAJOR)
BIN_NAME = $(LIBNAME).so
CFLAGS += -W -Wall -Wextra -std=c11 -Wcast-align -Wcast-qual -Wconversion -Wformat-nonliteral -Wformat-security \
	 -Wformat-signedness -Wformat-y2k -Wuninitialized -Wjump-misses-init -Wlogical-op -Wmissing-declarations \
	 -Wmissing-prototypes -Wstrict-prototypes -Wpointer-arith -Wswitch-default -Wswitch-enum -Wtrampolines \
	 -Wundef -Wwrite-strings

CFLAGS_SO = -fPIC $(CFLAGS)
ifdef DEBUG
	CFLAGS += -g3
else
	CFLAGS += -O3
	CFLAGS_SO  += -Wl,-s
endif

DESTDIR = 
PREFIX = $(DESTDIR)/usr
BINDIR = $(PREFIX)/bin
LIBDIR = $(PREFIX)/lib
INCLUDEDIR = $(PREFIX)/include
DATADIR = $(PREFIX)/shared
MANDIR = $(DATADIR)/man

all: $(BIN)

$(BIN): $(OBJS)
	${CROSS_COMPILE}$(CC) -shared -o $(BIN) -Wl,-soname,$(BIN) $(OBJS)

%.o: %.c %.h
	${CROSS_COMPILE}$(CC) -c $(CFLAGS_SO) $< -o $@

clean:
	$(RM) $(OBJS)
	$(RM) $(BIN)

install: $(BIN)
	mkdir -p $(LIBDIR)
	install $(INSTALL) $(BIN) $(LIBDIR)
	ln -s $(LIBDIR)/$(BIN) $(LIBDIR)/$(BIN_MAJOR)
	ln -s $(LIBDIR)/$(BIN_MAJOR) $(LIBDIR)/$(BIN_NAME)
	install -m 644 $(HEADER) $(INCLUDEDIR)

uninstall:
	$(RM) $(LIBDIR)/$(BIN)
	$(RM) $(LIBDIR)/$(BIN_MAJOR)
	$(RM) $(LIBDIR)/$(BIN_NAME)
	$(RM) $(INCLUDEDIR)/$(HEADER)
