#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>

// We should be very careful about which functions we use because they
// themselves could use malloc/free. Especially functions using FILE *
// since they might use buffered output. stderr seems to be fine because
// it is generally unbuffered.

static void *memory[1024] = {NULL};

static void
finished(void) {
	for(void **mem = memory; *mem; ++mem) {
		fprintf(stderr, "leaked memory %p\n", *mem);
	}
}

void*
malloc(size_t size)
{
	static void *(*real_malloc)(size_t) = NULL;
	if(!real_malloc) {
		real_malloc = dlsym(RTLD_NEXT, "malloc");
		atexit(finished);
	}
	void *p = real_malloc(size);
	if(p) {
		void **mem = memory;
		while(*mem) {
			++mem;
		}
		mem[0] = p;
		mem[1] = NULL;
	}
	return p;
}


void
free(void* ptr)
{
	static void (*real_free)(void *) = NULL;
	if(!real_free) {
		real_free = dlsym(RTLD_NEXT, "free");
	}
	free(ptr);
	void **mem = memory;
	while(*mem) {
		if(*mem == ptr) {
			// move everything after here left
			do {
				mem[0] = mem[1];
			} while(*++mem);
			return;
		}
	}
	fprintf(stderr, "invalid free %p\n", ptr);
}
