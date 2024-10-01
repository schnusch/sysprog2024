CC = cc
CPPFLAGS =
CFLAGS = -std=c99 -O2 -g -Wall -Wextra -Wpedantic -Wvla
LDFLAGS =

source_files = \
	main.c

.PHONY: all check clean

all: find

check: find
	$(MAKE) -C test

clean:
	$(RM) find
	$(MAKE) -C test $@

find: $(source_files)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $(source_files) $(LDFLAGS)
