CC = gcc
PKGCONF = pkgconf
CPPFLAGS = -DTRACE_ERRNO_BACKUP -DTRACE_FILE_DESCRIPTORS
CFLAGS = $(shell $(PKGCONF) --cflags readline)
LDFLAGS = $(shell $(PKGCONF) --libs readline)

cppflags = $(CPPFLAGS)
cflags = -std=c99 -O2 -g -Wall -Wextra -Wpedantic -Wvla $(CFLAGS)
ldflags = $(LDFLAGS)

source_files = \
	backup-errno.h \
	exec.c \
	exec.h \
	main.c \
	parse.c \
	parse.h

.PHONY: all clean

all: trash

clean:
	$(RM) trash

trash: $(source_files)
	$(CC) $(cppflags) $(cflags) -o $@ $(source_files) $(ldflags)
