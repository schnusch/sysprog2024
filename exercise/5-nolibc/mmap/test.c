#define _GNU_SOURCE
#include <stddef.h>
#include <stdlib.h>
#include <sys/mman.h>

#define PRINT_CONST(x)  printf(#x " = %d\n", x)

int main(void) {
	mmap(NULL, 1024, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	PRINT_CONST(PROT_READ);
	PRINT_CONST(PROT_WRITE);
	PRINT_CONST(MAP_PRIVATE);
	PRINT_CONST(MAP_ANONYMOUS);
	return 0;
}

