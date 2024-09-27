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

unsigned int strlen(const char *str) {
    unsigned int len = 0;
    while(*str++)
        len++;
    return len;
}

void *memcpy(void *dst, const void *src, unsigned int size) {
    char *d = (char*)dst;
    char *s = (char*)src;
    char *end = d + size;
    while(d != end) {
        *d = *s;
        d++;
        s++;
    }
    return dst;
}

void *realloc(void *ptr, unsigned int old_size, unsigned int new_size) {
    void *new_ptr = malloc(new_size);
    if(!new_ptr)
        return NULL;
    memcpy(new_ptr, ptr, old_size);
    return new_ptr;
}

char *read_line(int *eof) {
    unsigned int cur_size = 16;
    unsigned int pos = 0;
    char *buf = (char*)malloc(cur_size);
    if(!buf)
        return NULL;

    char c;
    int res;
    while((res = read(0, &c, 1)) > 0) {
        if(c == '\n')
            break;
        if(pos + 1 >= cur_size) {
            unsigned int new_size = cur_size * 2;
            char *new_buf = (char*)realloc(buf, cur_size, new_size);
            if(!new_buf)
                return NULL;
            cur_size = new_size;
            buf = new_buf;
        }
        buf[pos] = c;
        pos++;
    }
    if(res == 0)
        *eof = 1;
    buf[pos] = '\0';
    return buf;
}

char **read_lines(unsigned int *count) {
    unsigned int line_size = 16;
    char **lines = (char**)malloc(line_size * sizeof(char*));
    if(!lines)
        return NULL;

    while(1) {
        int eof = 0;
        char *line = read_line(&eof);
        if(!line)
            return NULL;
        if(eof)
            break;
        if(*count >= line_size) {
            unsigned int new_size = line_size * 2;
            char **new_buf = (char**)realloc(lines, line_size * sizeof(char*), new_size * sizeof(char*));
            if(!new_buf)
                return NULL;
            line_size = new_size;
            lines = new_buf;
        }
        lines[*count] = line;
        *count += 1;
    }
    return lines;
}

int main() {
    unsigned int count = 0;
    char **lines = read_lines(&count);
    print_num(count);
    write(1, "\n", 1);
    for(unsigned int i = 0; i < count; ++i) {
        write(1, lines[i], strlen(lines[i]));
        write(1, "\n", 1);
    }
    return 0;
}

