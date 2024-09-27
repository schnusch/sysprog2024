#include <sys/syscall.h>

void exit(int code) {
	long v = SYS_exit;
	asm volatile(
		"int $0x80"
		: // nothing
		: "a"(v), "b"(code)
	);
	__builtin_unreachable();
}

int main(void) {
	return 2;
}
