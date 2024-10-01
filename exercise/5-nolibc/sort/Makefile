include ../mmap/Makefile

run: $(BINARY) stdlib.h
	env LANG=C sort < stdlib.h | diff -u - sorted.txt

sorted.txt: $(BINARY) stdlib.h
	strace -e !write ./$(BINARY) < stdlib.h > sorted.txt

stdlib.h:
	src=$$(echo '#include <stdlib.h>' | gcc -E -Wp,-v - 2> /dev/null | grep -Pom 1 '(?<=").*/stdlib\.h(?=")') && ln -fs "$$src" $@
