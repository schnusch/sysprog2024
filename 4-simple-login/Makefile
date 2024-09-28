CFLAGS = -std=c99 -O2 -g -Wall -Wextra -Wpedantic -Wvla
CLANG_TIDY_CHECKS = llvm-include-order,misc-include-cleaner

.PHONY: \
	clean \
	gdb-overflow-exec \
	gdb-overflow-exit-0 \
	gdb-overflow-exit-1 \
	gdb-overflow-exit-2 \
	gdb-overflow-nothing \
	gdb-overflow-sigsegv \
	gdb-tui \
	test-overflow-exec \
	test-overflow-exit-0 \
	test-overflow-exit-1 \
	test-overflow-exit-2 \
	test-overflow-nothing \
	test-overflow-sigsegv \
	test-password

gdb-tui: gdb/tui.txt
	gdb -x gdb/tui.txt ./simple_login

overflow: overflow.c
	-clang-tidy --checks=$(CLANG_TIDY_CHECKS) overflow.c -- $(CFLAGS)
	$(CC) $(CFLAGS) -o $@ overflow.c

clean:
	$(RM) overflow overflow.txt overflow-exec.txt overflow-exit-0.txt overflow-exit-1.txt overflow-exit-2.txt overflow-nothing.txt overflow-sigsegv.txt

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

test-overflow-nothing: overflow-nothing.txt
	./simple_login < overflow-nothing.txt
gdb-overflow-nothing: gdb/overflow.txt overflow-nothing.txt
	ln -Tfs overflow-nothing.txt overflow.txt
	gdb -x gdb/overflow.txt ./simple_login
overflow-nothing.txt: overflow Makefile
	./overflow -b 0xffffdcc8 0x08048455 > .tmp.$@
	mv .tmp.$@ $@
	-xxd $@

# run gdb with input passwords that makes simple_login exit with 0
test-overflow-exit-0: overflow-exit-0.txt
	./simple_login < overflow-exit-0.txt
gdb-overflow-exit-0: gdb/overflow.txt overflow-exit-0.txt
	ln -Tfs overflow-exit-0.txt overflow.txt
	gdb -x gdb/overflow.txt ./simple_login
overflow-exit-0.txt: overflow Makefile
	# jump to unwinding of main, see $ objdump -Mintel -d simple_login
	./overflow 0x08048489 > .tmp.$@
	mv .tmp.$@ $@
	-xxd $@

# run gdb with input passwords that makes simple_login exit with 1
test-overflow-exit-1: overflow-exit-1.txt
	./simple_login < overflow-exit-1.txt
gdb-overflow-exit-1: gdb/overflow.txt overflow-exit-1.txt
	ln -Tfs overflow-exit-1.txt overflow.txt
	gdb -x gdb/overflow.txt ./simple_login
overflow-exit-1.txt: overflow Makefile
	# jump to unwinding of main, see $ objdump -Mintel -d simple_login
	./overflow -v 0x08048489 > .tmp.$@
	mv .tmp.$@ $@
	-xxd $@

# run gdb with input passwords that makes simple_login call exit(2)
test-overflow-exit-2: overflow-exit-2.txt
	# we have to turn off ASLR to have exit at the expected address
	setarch -R ./simple_login < overflow-exit-2.txt
gdb-overflow-exit-2: gdb/overflow-exec.txt overflow-exit-2.txt
	ln -Tfs overflow-exit-2.txt overflow.txt
	gdb -x gdb/overflow.txt ./simple_login
overflow-exit-2.txt: overflow Makefile
	./overflow -e2 0xf7dcfd50 > .tmp.$@
	mv .tmp.$@ $@
	-xxd $@

# run gdb with input passwords that makes simple_login call execlp(...)
test-overflow-exec: overflow-exec.txt
	# does not seem to work, even with ASLR off
	setarch -R strace ./simple_login < overflow-exec.txt
gdb-overflow-exec: gdb/overflow-exec.txt overflow-exec.txt
	gdb -x gdb/overflow-exec.txt ./simple_login
overflow-exec.txt: overflow Makefile
	./overflow -x 0xf7e74470 objdump -d -Mintel simple_login > .tmp.$@
	mv .tmp.$@ $@
	-xxd $@
