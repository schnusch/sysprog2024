CFLAGS = -std=c99 -O2 -g -Wall -Wextra -Wpedantic -Wvla
CLANG_TIDY_CHECKS = llvm-include-order,misc-include-cleaner

.PHONY: \
	clean \
	gdb-overflow-sigsegv \
	gdb-tui \
	test-overflow-sigsegv \
	test-password

gdb-tui: gdb/tui.txt
	gdb -x gdb/tui.txt ./simple_login

overflow: overflow.c
	-clang-tidy --checks=$(CLANG_TIDY_CHECKS) overflow.c -- $(CFLAGS)
	$(CC) $(CFLAGS) -o $@ overflow.c

clean:
	$(RM) overflow overflow.txt overflow-sigsegv.txt

test-password:
	./simple_login < password.txt

test-overflow-sigsegv: overflow-sigsegv.txt
	./simple_login < overflow-sigsegv.txt
gdb-overflow-sigsegv: gdb/overflow.txt overflow-sigsegv.txt
	ln -Tfs overflow-sigsegv.txt overflow.txt
	gdb -x gdb/overflow.txt ./simple_login
overflow-sigsegv.txt: overflow Makefile
	./overflow 0xFFFFFFFF > .tmp.$@
	mv .tmp.$@ $@
	-xxd $@
