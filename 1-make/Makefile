CC = cc
CFLAGS = -std=c99 -O2 -g -Wall -Wextra -Wpedantic -Wvla
LDFLAGS = -lm

srcs = main.c sincos.c
objs = $(srcs:.c=.o)

.POSIX:
.PHONY: all clean

all: sincos

clean:
	$(RM) sincos *.d *.o

sincos: $(objs)

%.d: %.c
	$(CC) -E -MM -o $@ $(@:.d=.c)
include $(srcs:.c=.d)
