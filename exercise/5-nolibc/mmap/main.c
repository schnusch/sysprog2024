#include "exit.h"
#include "mmap.h"

int main(void) {
	unsigned long n = 1024;
	char *p = malloc(n);
	while(n-- > 0) {
		*p++ = '0';
	}
	return 0;
}
