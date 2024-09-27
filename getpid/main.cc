#include <sys/syscall.h>

static int getpid() {
    int v = SYS_getpid;
    asm volatile (
        "int $0x80"
        : "+a"(v)
    );
    return v;
}

static unsigned int write(int fd, const char *buffer, unsigned int count) {
    unsigned int v = SYS_write;
    asm volatile (
        "int $0x80"
        : "+a"(v) : "b"(fd), "c"(buffer), "d"(count)
    );
    return v;
}

extern "C" void exit(int code) {
    int v = SYS_exit;
    asm volatile (
        "int $0x80"
        :: "a"(v), "b"(code)
    );
}

int main() {
    getpid();
    write(1, "Hello World!\n", 13);
    return 0;
}

