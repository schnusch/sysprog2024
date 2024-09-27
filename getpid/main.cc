#include <sys/syscall.h>

static int getpid() {
    int v = SYS_getpid;
    asm volatile (
        "int $0x80"
        : "+a"(v)
    );
    return v;
}

static int read(int fd, char *buffer, unsigned int count) {
    int v = SYS_read;
    asm volatile (
        "int $0x80"
        : "+a"(v) : "b"(fd), "c"(buffer), "d"(count)
    );
    return v;
}

static int write(int fd, const char *buffer, unsigned int count) {
    int v = SYS_write;
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
    __builtin_unreachable();
}

void print_num(int num) {
    if(num >= 10) {
        print_num(num / 10);
    }
    char digit = '0' + (num % 10);
    write(1, &digit, 1);
}

int main() {
    int lines = 0;
    char c;
    while(read(0, &c, 1) > 0) {
        if(c == '\n')
            lines++;
    }
    print_num(lines);
    write(1, "\n", 1);
    return 0;
}

