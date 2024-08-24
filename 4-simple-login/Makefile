.PHONY: \
	gdb-tui \
	test-password

gdb-tui: gdb/tui.txt
	gdb -x gdb/tui.txt ./simple_login

test-password:
	./simple_login < password.txt
