#include <stddef.h>  // NULL
#include <sys/mman.h>
#include <sys/syscall.h>

void *mmap(
	void *addr,
	unsigned int length,
	int prot,
	int flags,
	int fd,
	long offset
) {
	long v = SYS_mmap2;
	// Since we only have enough registers for syscalls with 5 arguments without
	// messing with %ebp, we could just ignore the last argument since we do
	// not intend to map a file.
	/*
	asm volatile(
		"int $0x80;"
		: "+a"(v)
		: "b"(addr), "c"(length), "d"(prot), "S"(flags), "D"(fd)
	);
	*/
	// The following code is adapted from musl libc
	// https://git.musl-libc.org/cgit/musl/tree/arch/i386/syscall_arch.h?h=v1.2.5&id=0784374d561435f7c787a555aeab8ede699ed298#n75
	asm volatile(
		"pushl %6 ;"
		"push %%ebp ;"
		"mov 4(%%esp),%%ebp ;"
		"int $0x80 ;"
		"pop %%ebp ;"
		"add $4,%%esp"
		: "+a"(v)
		: "b"(addr), "c"(length), "d"(prot), "S"(flags), "D"(fd), "g"(offset)
	);
	if((unsigned long)v > -4096UL) {
		return NULL;
	} else {
		return (void *)v;
	}
}

#define PAGE_SIZE 4096

void *malloc(unsigned long size) {
	unsigned long mmap_size = (size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
	void *p = mmap(NULL, mmap_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	// We return `p` so its end aligns with the page end so we get more
	// segfaults when we are careless.
	return p + (mmap_size - size);
}
