CC = cc
CPPFLAGS =
CFLAGS = -std=c99 -O2 -g -Wall -Wextra -Wpedantic -Wvla
LDFLAGS =

source_files = \
	main.c

.PHONY: all clean

all: find

clean:
	$(RM) find

find: $(source_files)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $(source_files) $(LDFLAGS)
