CC = gcc
PKGCONF = pkgconf
CPPFLAGS =
CFLAGS = $(shell $(PKGCONF) --cflags readline)
LDFLAGS = $(shell $(PKGCONF) --libs readline)

cppflags = $(CPPFLAGS)
cflags = -std=c99 -O2 -g -Wall -Wextra -Wpedantic -Wvla $(CFLAGS)
ldflags = $(LDFLAGS)

source_files = \
	main.c \
	parse.c \
	parse.h

.PHONY: all clean

all: trash

clean:
	$(RM) trash

trash: $(source_files)
	$(CC) -o $@ $(cppflags) $(cflags) $(source_files) $(ldflags)
