BINARY = main
OBJ = ${BINARY}.o start.o
CFLAGS = -std=gnu99 -Wall -Wextra -m32 -static -fno-PIC -O0 -g
ASFLAGS = --32
LDFLAGS = -nostdlib -m32

all: $(BINARY)

run: $(BINARY)
	./$(BINARY) < /etc/os-release

$(BINARY): $(OBJ)

clean:
	$(RM) $(OBJ) $(BINARY)

$(BINARY).o: exit.h read-write.h
