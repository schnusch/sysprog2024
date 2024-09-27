#include <sys/syscall.h>

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

void exit(int code) {
	long v = SYS_exit;
	asm volatile(
		"int $0x80"
		: // nothing
		: "a"(v), "b"(code)
	);
	__builtin_unreachable();
}

static const int STDIN_FILENO = 0;
static const int STDOUT_FILENO = 1;

int main(void) {
	char buffer[1024];
	long line_count = 0;

	long nread;
	while((nread = read(STDIN_FILENO, buffer, sizeof(buffer))) > 0) {
		for(const char *p = buffer; p < buffer + nread; ++p) {
			if(*p == '\n') {
				++line_count;
			}
		}
	}
	if(nread < 0) {
		return 1;
	}

	// We write from end of buffer backwards, because that is how we convert
	// int to string.
	// The resulting string will be at `p` until end of `buffer`.
	char *p = buffer + sizeof(buffer);
	*--p = '\n';
	do {
		if(p <= buffer) {
			return 1;
		}
		*--p = '0' + (line_count % 10);
		line_count /= 10;
	} while(line_count > 0);

	if(write(STDOUT_FILENO, p, buffer + sizeof(buffer) - p) < 0) {
		return 1;
	} else {
		return 0;
	}
}
