#include <sys/syscall.h>

static const int STDIN_FILENO = 0;
static const int STDOUT_FILENO = 1;

long read(int fd, char *buffer, long size) {
	long v = SYS_read;
	asm volatile(
		"int $0x80"
		: "+a"(v)
		: "b"(fd), "c"(buffer), "d"(size)
	);
	return v;
}

long write(int fd, const char *buffer, long size) {
	long v = SYS_write;
	asm volatile(
		"int $0x80"
		: "+a"(v)
		: "b"(fd), "c"(buffer), "d"(size)
	);
	return v;
}
