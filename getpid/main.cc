#include <sys/syscall.h>

static int getpid() {
    int v = SYS_getpid;
    asm volatile (
        "int $0x80"
        : "+a"(v)
    );
    return v;
}

/* write a program */
extern "C" void _start() {
    getpid();
    while(1)
        ;
}

