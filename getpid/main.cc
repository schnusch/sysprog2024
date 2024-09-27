#include <sys/syscall.h>
#include <sys/mman.h>
#include <stddef.h>

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

void *mmap(unsigned int size) {
    int v = SYS_mmap2;
    asm volatile (
        "int $0x80"
        : "+a"(v)
        : "b"(0),
          "c"(size),
          "d"(PROT_READ | PROT_WRITE),
          "S"(MAP_PRIVATE | MAP_ANONYMOUS),
          "D"(-1)
    );
    if(v < 0 && v > -4096)
        return NULL;
    return (void*)v;
}

void *malloc(unsigned int size) {
    static void *cur = NULL;
    static unsigned int rem = 0;
    if(rem < size) {
        unsigned int amount = size < 1024 * 1024 ? 1024 * 1024 : size;
        void *res = mmap(amount);
        if(!res)
            return NULL;
        cur = res;
        rem = amount;
    }

    void *res = cur;
    cur = (void*)((unsigned int)cur + size);
    rem -= size;
    return res;
}

int main() {
    void *test = malloc(4);

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

