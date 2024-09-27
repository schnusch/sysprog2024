include ../mmap/Makefile

run: $(BINARY)
	env LANG=C sort < /usr/include/stdlib.h | diff -u - sorted.txt

sorted.txt: $(BINARY)
	strace -e !write ./$(BINARY) < /usr/include/stdlib.h > sorted.txt
